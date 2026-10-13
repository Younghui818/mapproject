#include "Scene3DRenderer.h"

#include <QDebug>
#include <QVBoxLayout>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QLineWidth>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QPointSize>
#include <Qt3DRender/QTechnique>
#include <algorithm>
#include <cmath>

namespace {
// 高度 → 彩虹色（低=红，高=蓝），用于肉眼确认高度映射
QVector3D heightRamp(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  QColor c = QColor::fromHslF((1.0f - t) * 0.66f, 1.0f, 0.5f);
  return QVector3D(c.redF(), c.greenF(), c.blueF());
}

// 地形配色（高程分层设色）：深谷绿 → 草黄 → 土褐 → 岩石灰 → 雪白
QVector3D terrainColor(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  struct Stop {
    float p, r, g, b;
  };
  static const Stop kStops[] = {
      {0.00f, 0.16f, 0.35f, 0.20f}, {0.25f, 0.27f, 0.51f, 0.24f},
      {0.45f, 0.59f, 0.67f, 0.35f}, {0.65f, 0.67f, 0.55f, 0.39f},
      {0.85f, 0.51f, 0.45f, 0.41f}, {1.00f, 0.96f, 0.98f, 1.00f}};
  for (int i = 0; i < 5; ++i) {
    if (t <= kStops[i + 1].p) {
      const float k = (t - kStops[i].p) / (kStops[i + 1].p - kStops[i].p);
      return QVector3D(kStops[i].r + (kStops[i + 1].r - kStops[i].r) * k,
                       kStops[i].g + (kStops[i + 1].g - kStops[i].g) * k,
                       kStops[i].b + (kStops[i + 1].b - kStops[i].b) * k);
    }
  }
  return QVector3D(kStops[5].r, kStops[5].g, kStops[5].b);
}

// 从高度图用中心差分求法线（点云没有法线，用这个补）
QVector3D normalFromHeight(const QImage& img, int c, int r, int s, float dx,
                           float dz, float maxH) {
  const int cols = img.width(), rows = img.height();
  const int cm = std::max(0, c - s), cp = std::min(cols - 1, c + s);
  const int rm = std::max(0, r - s), rp = std::min(rows - 1, r + s);
  const float hL = qGray(img.pixel(cm, r)) / 255.0f * maxH;
  const float hR = qGray(img.pixel(cp, r)) / 255.0f * maxH;
  const float hD = qGray(img.pixel(c, rm)) / 255.0f * maxH;
  const float hU = qGray(img.pixel(c, rp)) / 255.0f * maxH;
  const float gx = (hR - hL) / std::max(1e-6f, float(cp - cm) * dx);
  const float gz = (hU - hD) / std::max(1e-6f, float(rp - rm) * dz);
  return QVector3D(-gx, 1.0f, -gz).normalized();
}

Qt3DCore::QAttribute* makeVertexAttr(Qt3DCore::QNode* parent,
                                     Qt3DCore::QBuffer* buf,
                                     const QString& name, uint vertexSize,
                                     uint count) {
  auto* a = new Qt3DCore::QAttribute(parent);
  a->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
  a->setName(name);
  a->setVertexBaseType(Qt3DCore::QAttribute::Float);
  a->setVertexSize(vertexSize);
  a->setByteStride(0);
  a->setCount(count);
  a->setBuffer(buf);
  return a;
}

QByteArray packVec3(const std::vector<QVector3D>& v) {
  QByteArray ba;
  ba.resize(static_cast<int>(v.size() * 3 * sizeof(float)));
  float* p = reinterpret_cast<float*>(ba.data());
  for (const auto& q : v) {
    *p++ = q.x();
    *p++ = q.y();
    *p++ = q.z();
  }
  return ba;
}
}  // namespace

Scene3DRenderer::Scene3DRenderer(QWidget* parent) : QWidget(parent) {
  setupScene();
}

void Scene3DRenderer::setupScene() {
  view_ = new Qt3DExtras::Qt3DWindow();
  view_->defaultFrameGraph()->setClearColor(QColor(40, 44, 52));

  container_ = QWidget::createWindowContainer(view_, this);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(container_);
  setLayout(layout);

  rootEntity_ = new Qt3DCore::QEntity();

  camera_ = view_->camera();
  camera_->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
  camera_->setPosition(QVector3D(0, 60, 80));
  camera_->setViewCenter(QVector3D(0, 0, 0));
  camera_->setUpVector(QVector3D(0, 1, 0));

  camController_ = new Qt3DExtras::QOrbitCameraController(rootEntity_);
  camController_->setCamera(camera_);
  camController_->setLookSpeed(180.0f);
  camController_->setLinearSpeed(50.0f);

  auto* lightEntity = new Qt3DCore::QEntity(rootEntity_);
  auto* light = new Qt3DRender::QPointLight(lightEntity);
  light->setColor("white");
  light->setIntensity(1.0f);
  auto* lightTransform = new Qt3DCore::QTransform(lightEntity);
  lightTransform->setTranslation(QVector3D(0, 50, 0));
  lightEntity->addComponent(light);
  lightEntity->addComponent(lightTransform);

  view_->setRootEntity(rootEntity_);
}

// ================= 显示方式切换 =================

void Scene3DRenderer::SetHeightData(const QImage& heightMap, double width,
                                    double depth, double maxHeight) {
  if (heightMap.isNull()) return;
  heightMap_ = heightMap;
  width_ = width;
  depth_ = depth;
  maxHeight_ = maxHeight;
  hasData_ = true;
  rebuild();
}

void Scene3DRenderer::SetShowType(Map3DShowType type) {
  if (showType_ == type && (pointCloudEntity_ || meshEntity_)) return;
  showType_ = type;
  rebuild();
}

void Scene3DRenderer::rebuild() {
  ClearAll();
  if (!hasData_) return;

  switch (showType_) {
    case kMap3DShowTypePoint:
      SetPointCloud(heightMap_, width_, depth_, maxHeight_, pointStep_);
      break;
    case kMap3DShowTypeMesh:
      SetHeightMesh(heightMap_, width_, depth_, maxHeight_, meshStep_,
                    wireframe_);
      break;
    case kMap3DShowTypeTexture:
      qDebug() << "[3D] texture mode not implemented yet";
      break;
  }
}

void Scene3DRenderer::SetPointStep(int step) {
  step = std::max(1, step);
  if (pointStep_ == step) return;
  pointStep_ = step;
  if (showType_ == kMap3DShowTypePoint) rebuild();
}

void Scene3DRenderer::SetMeshStep(int step) {
  step = std::max(1, step);
  if (meshStep_ == step) return;
  meshStep_ = step;
  if (showType_ == kMap3DShowTypeMesh) rebuild();
}

void Scene3DRenderer::SetWireframe(bool on) {
  if (wireframe_ == on) return;
  wireframe_ = on;
  if (showType_ == kMap3DShowTypeMesh) rebuild();
}

void Scene3DRenderer::SetPointSize(float px) {
  pointSize_ = px;
  applyPointSize();
}

void Scene3DRenderer::applyPointSize() {
  for (auto* ps : pointSizeNodes_) {
    if (ps) ps->setValue(pointSize_);
  }
}

void Scene3DRenderer::ClearAll() {
  ClearPointCloud();
  ClearMesh();
}

// ================= 通用销毁 =================

void Scene3DRenderer::destroyEntity(Qt3DCore::QEntity*& e) {
  if (!e) return;
  Qt3DCore::QEntity* victim = e;
  e = nullptr;  // 先摘成员，防止重入

  victim->setEnabled(false);
  for (auto* c : victim->components()) victim->removeComponent(c);
  victim->deleteLater();  // 延后删，等渲染 aspect 收尾
}

void Scene3DRenderer::frameCamera(double width, double depth,
                                  double maxHeight) {
  const float dist = static_cast<float>(std::max(width, depth)) * 1.6f;
  const float hOff = static_cast<float>(maxHeight) * 1.2f + dist * 0.4f;
  camera_->setViewCenter(QVector3D(0, static_cast<float>(maxHeight) * 0.3f, 0));
  camera_->setPosition(QVector3D(0, hOff, dist));
  camera_->setUpVector(QVector3D(0, 1, 0));
}

// ================= 点云 =================

void Scene3DRenderer::ClearPointCloud() {
  pointSizeNodes_.clear();
  destroyEntity(pointCloudEntity_);
}

void Scene3DRenderer::SetPointCloud(const QImage& heightMap, double width,
                                    double depth, double maxHeight, int step) {
  if (heightMap.isNull() || step < 1) return;
  const int cols = heightMap.width();
  const int rows = heightMap.height();

  std::vector<QVector3D> pts, clr;
  pts.reserve((rows / step + 1) * (cols / step + 1));
  clr.reserve(pts.capacity());

  for (int r = 0; r < rows; r += step) {
    for (int c = 0; c < cols; c += step) {
      const float h =
          qGray(heightMap.pixel(c, r)) / 255.0f * static_cast<float>(maxHeight);
      const float x = static_cast<float>(c) / (cols - 1) * width - width / 2.0;
      const float z = static_cast<float>(r) / (rows - 1) * depth - depth / 2.0;
      pts.emplace_back(x, h, z);

      const float t =
          (maxHeight > 0) ? h / static_cast<float>(maxHeight) : 0.0f;
      QVector3D col = terrainColor(t);
      if (hillshade_) {
        const float dx = static_cast<float>(width / (cols - 1));
        const float dz = static_cast<float>(depth / (rows - 1));
        col = shade(col, normalFromHeight(heightMap, c, r, step, dx, dz,
                                          static_cast<float>(maxHeight)));
      }
      clr.push_back(col);

      // clr.push_back(heightRamp(maxHeight > 0 ? h / maxHeight : 0.0f));
    }
  }
  SetPointCloud(pts, clr);
}

void Scene3DRenderer::SetPointCloud(const std::vector<QVector3D>& points,
                                    const std::vector<QVector3D>& colors) {
  ClearPointCloud();
  if (points.empty() || !rootEntity_) return;
  const int n = static_cast<int>(points.size());

  QByteArray posData, colData;
  posData.resize(n * 3 * static_cast<int>(sizeof(float)));
  colData.resize(n * 3 * static_cast<int>(sizeof(float)));
  {
    float* p = reinterpret_cast<float*>(posData.data());
    float* q = reinterpret_cast<float*>(colData.data());
    const bool hasColor = (colors.size() == points.size());
    for (int i = 0; i < n; ++i) {
      *p++ = points[i].x();
      *p++ = points[i].y();
      *p++ = points[i].z();
      if (hasColor) {
        *q++ = colors[i].x();
        *q++ = colors[i].y();
        *q++ = colors[i].z();
      } else {
        *q++ = 0.9f;
        *q++ = 0.9f;
        *q++ = 0.2f;
      }
    }
  }

  auto* posBuf = new Qt3DCore::QBuffer();
  posBuf->setData(posData);
  auto* colBuf = new Qt3DCore::QBuffer();
  colBuf->setData(colData);

  auto* renderer = new Qt3DRender::QGeometryRenderer();
  auto* geom = new Qt3DCore::QGeometry(renderer);
  posBuf->setParent(geom);
  colBuf->setParent(geom);
  geom->addAttribute(makeVertexAttr(
      geom, posBuf, Qt3DCore::QAttribute::defaultPositionAttributeName(), 3,
      uint(n)));
  geom->addAttribute(makeVertexAttr(
      geom, colBuf, Qt3DCore::QAttribute::defaultColorAttributeName(), 3,
      uint(n)));

  renderer->setGeometry(geom);
  renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Points);
  renderer->setFirstVertex(0);
  renderer->setVertexCount(n);

  pointCloudEntity_ = new Qt3DCore::QEntity(rootEntity_);
  renderer->setParent(pointCloudEntity_);
  pointCloudEntity_->addComponent(renderer);

  // 官方顶点色材质 + 注入 QPointSize(Fixed)：绕开自定义着色器
  auto* mat = new Qt3DExtras::QPerVertexColorMaterial(pointCloudEntity_);
  pointCloudEntity_->addComponent(mat);
  if (auto* eff = mat->effect()) {
    for (auto* tech : eff->techniques()) {
      for (auto* pass : tech->renderPasses()) {
        auto* ps = new Qt3DRender::QPointSize(pass);
        ps->setSizeMode(Qt3DRender::QPointSize::Fixed);
        ps->setValue(pointSize_);
        pass->addRenderState(ps);
        pointSizeNodes_.append(ps);
      }
    }
  }

  qDebug() << "--- Point cloud --- points:" << n << "size:" << pointSize_;
}

// ================= 网格 =================

void Scene3DRenderer::ClearMesh() { destroyEntity(meshEntity_); }

void Scene3DRenderer::SetHeightMesh(const QImage& heightMap, double width,
                                    double depth, double maxHeight, int step,
                                    bool wireframe) {
  ClearMesh();
  if (heightMap.isNull() || step < 1 || !rootEntity_) return;

  const int cols = heightMap.width();
  const int rows = heightMap.height();
  if (cols < 2 || rows < 2) return;

  const int sc = std::max(2, cols / step);
  const int sr = std::max(2, rows / step);
  const int n = sc * sr;

  std::vector<QVector3D> pos(n), clr(n), nrm(n, QVector3D(0, 1, 0));

  // 1. 顶点
  for (int rr = 0; rr < sr; ++rr) {
    const int r = std::min(rr * step, rows - 1);
    for (int cc = 0; cc < sc; ++cc) {
      const int c = std::min(cc * step, cols - 1);
      const float h =
          qGray(heightMap.pixel(c, r)) / 255.0f * static_cast<float>(maxHeight);
      const float x = static_cast<float>(cc) / (sc - 1) * width - width / 2.0;
      const float z = static_cast<float>(rr) / (sr - 1) * depth - depth / 2.0;
      const int i = rr * sc + cc;
      pos[i] = QVector3D(x, h, z);
      clr[i] = terrainColor(maxHeight > 0 ? h / static_cast<float>(maxHeight)
                                          : 0.0f);
      // clr[i] =
      //     heightRamp(maxHeight > 0 ? h / static_cast<float>(maxHeight) :
      //     0.0f);
    }
  }

  // 2. 索引（CCW，法线朝 +Y）
  std::vector<unsigned int> idx;
  if (wireframe) {
    idx.reserve((sr - 1) * (sc - 1) * 8);
    for (int rr = 0; rr < sr - 1; ++rr) {
      for (int cc = 0; cc < sc - 1; ++cc) {
        const unsigned int i00 = rr * sc + cc;
        const unsigned int i10 = (rr + 1) * sc + cc;
        const unsigned int i01 = rr * sc + (cc + 1);
        const unsigned int i11 = (rr + 1) * sc + (cc + 1);
        idx.insert(idx.end(), {i00, i01, i01, i11, i11, i10, i10, i00});
      }
    }
  } else {
    idx.reserve((sr - 1) * (sc - 1) * 6);
    for (int rr = 0; rr < sr - 1; ++rr) {
      for (int cc = 0; cc < sc - 1; ++cc) {
        const int i00 = rr * sc + cc;
        const int i10 = (rr + 1) * sc + cc;
        const int i01 = rr * sc + (cc + 1);
        const int i11 = (rr + 1) * sc + (cc + 1);
        idx.insert(idx.end(), {unsigned(i00), unsigned(i10), unsigned(i01)});
        idx.insert(idx.end(), {unsigned(i01), unsigned(i10), unsigned(i11)});

        // 面积加权法线（不归一化即自带面积权重，最后统一归一化）
        const QVector3D n1 =
            QVector3D::crossProduct(pos[i10] - pos[i00], pos[i01] - pos[i00]);
        const QVector3D n2 =
            QVector3D::crossProduct(pos[i11] - pos[i01], pos[i10] - pos[i01]);
        nrm[i00] += n1;
        nrm[i10] += n1;
        nrm[i01] += n1;
        nrm[i01] += n2;
        nrm[i10] += n2;
        nrm[i11] += n2;
      }
    }
    for (auto& v : nrm) v.normalize();
    if (hillshade_) {
      for (int i = 0; i < n; ++i) clr[i] = shade(clr[i], nrm[i]);
    }
  }

  // 3. Buffer
  auto* posBuf = new Qt3DCore::QBuffer();
  posBuf->setData(packVec3(pos));
  auto* colBuf = new Qt3DCore::QBuffer();
  colBuf->setData(packVec3(clr));
  auto* nrmBuf = new Qt3DCore::QBuffer();
  nrmBuf->setData(packVec3(nrm));
  auto* idxBuf = new Qt3DCore::QBuffer();
  {
    QByteArray ba;
    ba.resize(static_cast<int>(idx.size() * sizeof(unsigned int)));
    std::copy(idx.begin(), idx.end(),
              reinterpret_cast<unsigned int*>(ba.data()));
    idxBuf->setData(ba);
  }

  // 4. Geometry / Renderer
  auto* renderer = new Qt3DRender::QGeometryRenderer();
  auto* geom = new Qt3DCore::QGeometry(renderer);
  posBuf->setParent(geom);
  colBuf->setParent(geom);
  nrmBuf->setParent(geom);
  idxBuf->setParent(geom);

  geom->addAttribute(makeVertexAttr(
      geom, posBuf, Qt3DCore::QAttribute::defaultPositionAttributeName(), 3,
      uint(n)));
  geom->addAttribute(makeVertexAttr(
      geom, colBuf, Qt3DCore::QAttribute::defaultColorAttributeName(), 3,
      uint(n)));
  geom->addAttribute(makeVertexAttr(
      geom, nrmBuf, Qt3DCore::QAttribute::defaultNormalAttributeName(), 3,
      uint(n)));

  auto* idxAttr = new Qt3DCore::QAttribute(geom);
  idxAttr->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
  idxAttr->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
  idxAttr->setVertexSize(1);
  idxAttr->setByteStride(0);
  idxAttr->setCount(static_cast<int>(idx.size()));
  idxAttr->setBuffer(idxBuf);
  geom->addAttribute(idxAttr);

  renderer->setGeometry(geom);
  renderer->setFirstVertex(0);
  renderer->setPrimitiveType(wireframe
                                 ? Qt3DRender::QGeometryRenderer::Lines
                                 : Qt3DRender::QGeometryRenderer::Triangles);
  renderer->setVertexCount(static_cast<int>(idx.size()));  // 有索引 → 用索引数

  // 5. 实体 + 材质
  meshEntity_ = new Qt3DCore::QEntity(rootEntity_);
  renderer->setParent(meshEntity_);
  meshEntity_->addComponent(renderer);

  auto* mat = new Qt3DExtras::QPerVertexColorMaterial(meshEntity_);
  meshEntity_->addComponent(mat);
  if (wireframe && mat->effect()) {
    for (auto* tech : mat->effect()->techniques())
      for (auto* pass : tech->renderPasses()) {
        auto* lw = new Qt3DRender::QLineWidth(pass);
        lw->setValue(1.0f);  // core profile 下宽线通常被忽略
        pass->addRenderState(lw);
      }
  }

  frameCamera(width, depth, maxHeight);
  qDebug() << "--- Mesh --- verts:" << n << "indices:" << idx.size()
           << "wireframe:" << wireframe;
}

void Scene3DRenderer::SetHillshade(bool on) {
  if (hillshade_ == on) return;
  hillshade_ = on;
  rebuild();
}

void Scene3DRenderer::SetShadeLight(float x, float y, float z) {
  lightDir_ = QVector3D(x, y, z).normalized();
  rebuild();
}

void Scene3DRenderer::SetAmbient(float v) {
  ambient_ = std::clamp(v, 0.0f, 1.0f);
  rebuild();
}

QVector3D Scene3DRenderer::shade(const QVector3D& base,
                                 const QVector3D& n) const {
  const float lambert = std::max(0.0f, QVector3D::dotProduct(n, lightDir_));
  const float k = ambient_ + (1.0f - ambient_) * lambert;
  return QVector3D(std::min(1.0f, base.x() * k), std::min(1.0f, base.y() * k),
                   std::min(1.0f, base.z() * k));
}