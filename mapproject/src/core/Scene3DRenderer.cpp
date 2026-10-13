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
struct GeoData {
  std::vector<QVector3D> pos, clr, nrm;
  std::vector<unsigned int> idx;
};

inline float at(const std::vector<float>& h, int cols, int rows, int c, int r) {
  c = std::clamp(c, 0, cols - 1);
  r = std::clamp(r, 0, rows - 1);
  return h[static_cast<size_t>(r) * cols + c];
}

// 地形分层设色：深谷绿 → 草黄 → 土褐 → 岩石灰 → 雪白
QVector3D terrainColor(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  struct Stop {
    float p, r, g, b;
  };
  static const Stop kS[] = {
      {0.00f, 0.16f, 0.35f, 0.20f}, {0.25f, 0.27f, 0.51f, 0.24f},
      {0.45f, 0.59f, 0.67f, 0.35f}, {0.65f, 0.67f, 0.55f, 0.39f},
      {0.85f, 0.51f, 0.45f, 0.41f}, {1.00f, 0.96f, 0.98f, 1.00f}};
  for (int i = 0; i < 5; ++i)
    if (t <= kS[i + 1].p) {
      const float k = (t - kS[i].p) / (kS[i + 1].p - kS[i].p);
      return QVector3D(kS[i].r + (kS[i + 1].r - kS[i].r) * k,
                       kS[i].g + (kS[i + 1].g - kS[i].g) * k,
                       kS[i].b + (kS[i + 1].b - kS[i].b) * k);
    }
  return QVector3D(kS[5].r, kS[5].g, kS[5].b);
}

QVector3D shadeColor(const QVector3D& base, const QVector3D& n,
                     const QVector3D& L, float ambient) {
  const float lam = std::max(0.0f, QVector3D::dotProduct(n, L));
  const float k = ambient + (1.0f - ambient) * lam;
  return QVector3D(std::min(1.0f, base.x() * k), std::min(1.0f, base.y() * k),
                   std::min(1.0f, base.z() * k));
}

// 中心差分法线（世界坐标，含垂直夸张）
QVector3D gridNormal(const std::vector<float>& h, int cols, int rows, int c,
                     int r, int s, float dx, float dz, float minH,
                     double exag) {
  auto Y = [&](int cc, int rr) {
    return static_cast<float>((at(h, cols, rows, cc, rr) - minH) * exag);
  };
  const float hl = Y(c - s, r), hr = Y(c + s, r);
  const float hd = Y(c, r - s), hu = Y(c, r + s);
  const float sx = float(std::min(c + s, cols - 1) - std::max(c - s, 0)) * dx;
  const float sz = float(std::min(r + s, rows - 1) - std::max(r - s, 0)) * dz;
  const float gx = (hr - hl) / std::max(1e-6f, sx);
  const float gz = (hu - hd) / std::max(1e-6f, sz);
  return QVector3D(-gx, 1.0f, -gz).normalized();
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

// ---------- 点云几何 ----------
GeoData buildPointCloud(const std::vector<float>& h, int cols, int rows,
                        double width, double depth, float minH, float maxH,
                        double exag, int step, bool hillshade,
                        const QVector3D& L, float ambient) {
  GeoData g;
  const double dx = width / std::max(1.0, double(cols - 1));
  const double dz = depth / std::max(1.0, double(rows - 1));
  const float span = std::max(1e-6f, maxH - minH);
  const int s = std::max(1, step);

  for (int r = 0; r < rows; r += s)
    for (int c = 0; c < cols; c += s) {
      const float hv = at(h, cols, rows, c, r);
      const float x = float(double(c) * dx - width / 2.0);
      const float z = float(double(r) * dz - depth / 2.0);
      const float y = float((hv - minH) * exag);
      QVector3D col = terrainColor((hv - minH) / span);
      if (hillshade)
        col = shadeColor(col,
                         gridNormal(h, cols, rows, c, r, s, float(dx),
                                    float(dz), minH, exag),
                         L, ambient);
      g.pos.emplace_back(x, y, z);
      g.clr.push_back(col);
      g.nrm.emplace_back(0.0f, 1.0f, 0.0f);
    }
  return g;
}

// ---------- 网格几何 ----------
GeoData buildMesh(const std::vector<float>& h, int cols, int rows, double width,
                  double depth, float minH, float maxH, double exag, int step,
                  bool wireframe, bool hillshade, const QVector3D& L,
                  float ambient) {
  GeoData g;
  const int sc = std::max(2, cols / std::max(1, step));
  const int sr = std::max(2, rows / std::max(1, step));
  const int n = sc * sr;
  const double dx = width / (sc - 1), dz = depth / (sr - 1);
  const float span = std::max(1e-6f, maxH - minH);

  g.pos.resize(n);
  g.clr.resize(n);
  g.nrm.assign(n, QVector3D(0, 1, 0));

  for (int rr = 0; rr < sr; ++rr)
    for (int cc = 0; cc < sc; ++cc) {
      const int c = std::min(cc * step, cols - 1);
      const int r = std::min(rr * step, rows - 1);
      const float hv = at(h, cols, rows, c, r);
      const int i = rr * sc + cc;
      g.pos[i] = QVector3D(float(double(cc) * dx - width / 2.0),
                           float((hv - minH) * exag),
                           float(double(rr) * dz - depth / 2.0));
      g.clr[i] = terrainColor((hv - minH) / span);
    }

  if (wireframe) {
    g.idx.reserve((sr - 1) * (sc - 1) * 8);
    for (int rr = 0; rr < sr - 1; ++rr)
      for (int cc = 0; cc < sc - 1; ++cc) {
        const unsigned int i00 = rr * sc + cc, i10 = (rr + 1) * sc + cc;
        const unsigned int i01 = rr * sc + cc + 1, i11 = (rr + 1) * sc + cc + 1;
        g.idx.insert(g.idx.end(), {i00, i01, i01, i11, i11, i10, i10, i00});
      }
  } else {
    g.idx.reserve((sr - 1) * (sc - 1) * 6);
    for (int rr = 0; rr < sr - 1; ++rr)
      for (int cc = 0; cc < sc - 1; ++cc) {
        const int i00 = rr * sc + cc, i10 = (rr + 1) * sc + cc;
        const int i01 = rr * sc + cc + 1, i11 = (rr + 1) * sc + cc + 1;
        g.idx.insert(g.idx.end(),
                     {unsigned(i00), unsigned(i10), unsigned(i01)});
        g.idx.insert(g.idx.end(),
                     {unsigned(i01), unsigned(i10), unsigned(i11)});
        const QVector3D n1 = QVector3D::crossProduct(g.pos[i10] - g.pos[i00],
                                                     g.pos[i01] - g.pos[i00]);
        const QVector3D n2 = QVector3D::crossProduct(g.pos[i11] - g.pos[i01],
                                                     g.pos[i10] - g.pos[i01]);
        g.nrm[i00] += n1;
        g.nrm[i10] += n1;
        g.nrm[i01] += n1;
        g.nrm[i01] += n2;
        g.nrm[i10] += n2;
        g.nrm[i11] += n2;
      }
    for (auto& v : g.nrm) v.normalize();
  }

  if (hillshade && !wireframe)
    for (int i = 0; i < n; ++i)
      g.clr[i] = shadeColor(g.clr[i], g.nrm[i], L, ambient);

  return g;
}
}  // namespace

// ============================================================

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
  camera_->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 0.1f, 1.0e6f);
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

// ---------- 数据入口 ----------

void Scene3DRenderer::SetHeightData(const QImage& img, double width,
                                    double depth, double maxHeight) {
  if (img.isNull()) return;
  std::vector<float> h(size_t(img.width()) * img.height());
  for (int y = 0; y < img.height(); ++y)
    for (int x = 0; x < img.width(); ++x)
      h[size_t(y) * img.width() + x] =
          qGray(img.pixel(x, y)) / 255.0f * float(maxHeight);
  SetHeightGrid(h, img.width(), img.height(), width, depth);
}

void Scene3DRenderer::SetHeightGrid(const std::vector<float>& heights, int cols,
                                    int rows, double width, double depth) {
  if (heights.empty() || cols < 2 || rows < 2) return;

  // 防御：地面尺寸为 0 时用分辨率兜底，避免相机塌到原点
  if (width <= 0.0 || depth <= 0.0) {
    const double fallback = 30.0;  // 默认 30 m/px
    width = width > 0 ? width : cols * fallback;
    depth = depth > 0 ? depth : rows * fallback;
    qWarning() << "[3D] ground size is 0, fallback to" << width << "x" << depth;
  }

  grid_ = heights;
  gridCols_ = cols;
  gridRows_ = rows;
  width_ = width;
  depth_ = depth;

  heightMin_ = *std::min_element(grid_.begin(), grid_.end());
  heightMax_ = *std::max_element(grid_.begin(), grid_.end());
  hasData_ = true;

  qDebug() << "[3D] grid" << cols << "x" << rows << "ground" << width << "x"
           << depth << "relief" << heightMin_ << "→" << heightMax_ << "(Δ"
           << (heightMax_ - heightMin_) << "m)";
  rebuild();
}

// ---------- 切换 ----------

void Scene3DRenderer::SetShowType(Map3DShowType type) {
  if (showType_ == type && (pointCloudEntity_ || meshEntity_)) return;
  showType_ = type;
  rebuild();
}

void Scene3DRenderer::rebuild() {
  ClearAll();
  if (!hasData_ || !rootEntity_) return;

  const float minH = float(heightMin_), maxH = float(heightMax_);

  if (showType_ == kMap3DShowTypePoint) {
    GeoData g = buildPointCloud(grid_, gridCols_, gridRows_, width_, depth_,
                                minH, maxH, exaggeration_, pointStep_,
                                hillshade_, lightDir_, ambient_);
    makeEntity(pointCloudEntity_, &g, true, false);
    qDebug() << "[3D] point cloud:" << g.pos.size();
  } else if (showType_ == kMap3DShowTypeMesh) {
    GeoData g = buildMesh(grid_, gridCols_, gridRows_, width_, depth_, minH,
                          maxH, exaggeration_, meshStep_, wireframe_,
                          hillshade_, lightDir_, ambient_);
    makeEntity(meshEntity_, &g, false, wireframe_);
    qDebug() << "[3D] mesh: verts" << g.pos.size() << "idx" << g.idx.size();
  }
  frameCamera();
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

void Scene3DRenderer::SetVerticalExaggeration(double k) {
  exaggeration_ = std::max(0.05, k);
  rebuild();
}

void Scene3DRenderer::SetPointSize(float px) {
  pointSize_ = px;
  applyPointSize();
}

void Scene3DRenderer::applyPointSize() {
  for (auto* ps : pointSizeNodes_)
    if (ps) ps->setValue(pointSize_);
}

void Scene3DRenderer::frameCamera() {
  const double relief =
      std::max(1.0, (heightMax_ - heightMin_) * exaggeration_);
  const double diag =
      std::sqrt(width_ * width_ + depth_ * depth_ + relief * relief);
  const float dist = float(std::max(width_, depth_)) * 1.25f;
  const float hOff = float(relief) * 0.9f + dist * 0.55f;

  camera_->setViewCenter(QVector3D(0, float(relief) * 0.35f, 0));
  camera_->setPosition(QVector3D(0, hOff, dist));
  camera_->setUpVector(QVector3D(0, 1, 0));

  // 关键：远近裁剪面随场景尺度走，否则大范围瓦片会被整个裁掉
  const float nearPlane = std::max(0.05f, dist * 0.0008f);
  const float farPlane = dist * 10.0f + float(diag) * 4.0f;
  float aspect = 16.0f / 9.0f;
  if (container_ && container_->height() > 0)
    aspect = float(container_->width()) / float(container_->height());
  camera_->lens()->setPerspectiveProjection(45.0f, aspect, nearPlane, farPlane);

  qDebug() << "[cam] dist" << dist << "near" << nearPlane << "far" << farPlane;
}

// ---------- 实体构建 / 销毁 ----------

void Scene3DRenderer::makeEntity(Qt3DCore::QEntity*& ent, const void* geoPtr,
                                 bool asPoints, bool wireframe) {
  const GeoData& g = *static_cast<const GeoData*>(geoPtr);
  const int n = int(g.pos.size());
  if (n == 0) return;

  auto* renderer = new Qt3DRender::QGeometryRenderer();
  auto* geom = new Qt3DCore::QGeometry(renderer);

  auto* posBuf = new Qt3DCore::QBuffer(geom);
  posBuf->setData(packVec3(g.pos));
  auto* colBuf = new Qt3DCore::QBuffer(geom);
  colBuf->setData(packVec3(g.clr));
  geom->addAttribute(makeVertexAttr(
      geom, posBuf, Qt3DCore::QAttribute::defaultPositionAttributeName(), 3,
      uint(n)));
  geom->addAttribute(makeVertexAttr(
      geom, colBuf, Qt3DCore::QAttribute::defaultColorAttributeName(), 3,
      uint(n)));

  if (!asPoints) {
    auto* nrmBuf = new Qt3DCore::QBuffer(geom);
    nrmBuf->setData(packVec3(g.nrm));
    geom->addAttribute(makeVertexAttr(
        geom, nrmBuf, Qt3DCore::QAttribute::defaultNormalAttributeName(), 3,
        uint(n)));

    auto* idxBuf = new Qt3DCore::QBuffer(geom);
    QByteArray ib;
    ib.resize(int(g.idx.size() * sizeof(unsigned int)));
    std::copy(g.idx.begin(), g.idx.end(),
              reinterpret_cast<unsigned int*>(ib.data()));
    idxBuf->setData(ib);

    auto* ia = new Qt3DCore::QAttribute(geom);
    ia->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
    ia->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
    ia->setVertexSize(1);
    ia->setByteStride(0);
    ia->setCount(int(g.idx.size()));
    ia->setBuffer(idxBuf);
    geom->addAttribute(ia);
  }

  renderer->setGeometry(geom);
  renderer->setFirstVertex(0);
  if (asPoints) {
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Points);
    renderer->setVertexCount(n);
  } else {
    renderer->setPrimitiveType(wireframe
                                   ? Qt3DRender::QGeometryRenderer::Lines
                                   : Qt3DRender::QGeometryRenderer::Triangles);
    renderer->setVertexCount(int(g.idx.size()));  // 有索引 → 用索引数
  }

  ent = new Qt3DCore::QEntity(rootEntity_);
  renderer->setParent(ent);
  ent->addComponent(renderer);

  auto* mat = new Qt3DExtras::QPerVertexColorMaterial(ent);
  ent->addComponent(mat);

  if (asPoints) {
    if (auto* eff = mat->effect())
      for (auto* tech : eff->techniques())
        for (auto* pass : tech->renderPasses()) {
          auto* ps = new Qt3DRender::QPointSize(pass);
          ps->setSizeMode(Qt3DRender::QPointSize::Fixed);
          ps->setValue(pointSize_);
          pass->addRenderState(ps);
          pointSizeNodes_.append(ps);
        }
  } else if (wireframe) {
    if (auto* eff = mat->effect())
      for (auto* tech : eff->techniques())
        for (auto* pass : tech->renderPasses()) {
          auto* lw = new Qt3DRender::QLineWidth(pass);
          lw->setValue(1.0f);
          pass->addRenderState(lw);
        }
  }
}

void Scene3DRenderer::destroyEntity(Qt3DCore::QEntity*& e) {
  if (!e) return;
  Qt3DCore::QEntity* victim = e;
  e = nullptr;
  victim->setEnabled(false);
  for (auto* c : victim->components()) victim->removeComponent(c);
  victim->deleteLater();
}

void Scene3DRenderer::ClearAll() {
  pointSizeNodes_.clear();
  destroyEntity(pointCloudEntity_);
  destroyEntity(meshEntity_);
}

void Scene3DRenderer::SetMesh(const MeshData& mesh) {}