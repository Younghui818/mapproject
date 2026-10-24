#include "Scene3DRenderer.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QVector2D>
#include <QVector4D>
#include <QWheelEvent>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QLineWidth>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QPointSize>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QTextureImage>
#include <algorithm>
#include <cmath>

namespace {
struct GeoData {
  std::vector<QVector3D> pos, clr, nrm;
  std::vector<QVector2D> uv;  // ← 新增
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
                        const QVector3D& L, float ambient, bool flipV) {
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

      const float vu = float(c) / float(cols - 1);
      const float vv = float(r) / float(rows - 1);
      g.uv.emplace_back(vu, flipV ? 1.0f - vv : vv);

      g.clr.push_back(col);
      g.nrm.emplace_back(0.0f, 1.0f, 0.0f);
    }
  return g;
}

// ---------- 网格几何 ----------
GeoData buildMesh(const std::vector<float>& h, int cols, int rows, double width,
                  double depth, float minH, float maxH, double exag, int step,
                  bool wireframe, bool hillshade, const QVector3D& L,
                  float ambient, bool flipV) {
  GeoData g;
  const int sc = std::max(2, cols / std::max(1, step));
  const int sr = std::max(2, rows / std::max(1, step));
  const int n = sc * sr;
  const double dx = width / (sc - 1), dz = depth / (sr - 1);
  const float span = std::max(1e-6f, maxH - minH);

  g.pos.resize(n);
  g.clr.resize(n);
  g.uv.resize(n);
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
      const float vu = float(cc) / float(sc - 1);
      const float vv = float(rr) / float(sr - 1);
      g.uv[i] = QVector2D(vu, flipV ? 1.0f - vv : vv);
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

class MemoryTextureImage : public Qt3DRender::QPaintedTextureImage {
 public:
  explicit MemoryTextureImage(const QImage& img,
                              Qt3DCore::QNode* parent = nullptr)
      : Qt3DRender::QPaintedTextureImage(parent), img_(img) {
    setSize(img.size());
  }
  void setImage(const QImage& img) {
    img_ = img;
    setSize(img.size());
    update();
  }

 protected:
  void paint(QPainter* painter) override { painter->drawImage(0, 0, img_); }

 private:
  QImage img_;
};
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
  camController_->setLinearSpeed(0.0f);  // ★ 置 0，平移完全交给 doPan

  auto* lightEntity = new Qt3DCore::QEntity(rootEntity_);
  auto* light = new Qt3DRender::QPointLight(lightEntity);
  light->setColor("white");
  light->setIntensity(1.0f);
  auto* lightTransform = new Qt3DCore::QTransform(lightEntity);
  lightTransform->setTranslation(QVector3D(0, 50, 0));
  lightEntity->addComponent(light);
  lightEntity->addComponent(lightTransform);

  view_->setRootEntity(rootEntity_);

  container_->installEventFilter(this);
  view_->installEventFilter(this);
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

  minDistance_ = std::max(1.0f, float(std::max(width, depth)) * 0.002f);
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
  const QVector3D L = lightDirFromAngles();

  if (showType_ == kMap3DShowTypePoint) {
    GeoData g = buildPointCloud(grid_, gridCols_, gridRows_, width_, depth_,
                                minH, maxH, exaggeration_, pointStep_,
                                hillshade_, lightDir_, ambient_, flipV_);
    makeEntity(pointCloudEntity_, &g, true, false);
    qDebug() << "[3D] point cloud:" << g.pos.size();
  } else if (showType_ == kMap3DShowTypeMesh) {
    GeoData g = buildMesh(grid_, gridCols_, gridRows_, width_, depth_, minH,
                          maxH, exaggeration_, meshStep_, wireframe_,
                          hillshade_, lightDir_, ambient_, flipV_);
    makeEntity(meshEntity_, &g, false, wireframe_);
    qDebug() << "[3D] mesh: verts" << g.pos.size() << "idx" << g.idx.size();
  }
  frameCamera();
  RefreshAllPathEntities();  // 路径跟着地形一起刷新
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

  if (!g.uv.empty()) {
    QByteArray uvData;
    uvData.resize(int(g.uv.size() * 2 * sizeof(float)));
    float* p = reinterpret_cast<float*>(uvData.data());
    for (const auto& t : g.uv) {
      *p++ = t.x();
      *p++ = t.y();
    }
    auto* uvBuf = new Qt3DCore::QBuffer(geom);
    uvBuf->setData(uvData);

    auto* uvAttr = new Qt3DCore::QAttribute(geom);
    uvAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    uvAttr->setName(
        Qt3DCore::QAttribute::defaultTextureCoordinateAttributeName());
    uvAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    uvAttr->setVertexSize(2);
    uvAttr->setByteStride(0);
    uvAttr->setCount(uint(g.uv.size()));
    uvAttr->setBuffer(uvBuf);
    geom->addAttribute(uvAttr);
  }
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

  if (useTexture_ && !terrainImage_.isNull() && !asPoints) {
    auto* tex = new Qt3DRender::QTexture2D(ent);
    auto* img = new MemoryTextureImage(terrainImage_, ent);
    terrainTexImage_ = img;  // ★ 新增
    tex->addTextureImage(img);
    tex->setMinificationFilter(Qt3DRender::QTexture2D::LinearMipMapLinear);
    tex->setMagnificationFilter(Qt3DRender::QTexture2D::Linear);
    tex->setGenerateMipMaps(true);
    tex->setWrapMode(Qt3DRender::QTextureWrapMode(
        Qt3DRender::QTextureWrapMode::ClampToEdge));

    auto* mat = new Qt3DExtras::QTextureMaterial(ent);
    mat->setTexture(tex);
    ent->addComponent(mat);
  } else {
    // ---- 顶点色模式（原逻辑不变）----
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
  terrainTexImage_ = nullptr;  // ★ 新增
  destroyEntity(pointCloudEntity_);
  destroyEntity(meshEntity_);
}

void Scene3DRenderer::SetTerrainTexture(const QImage& image) {
  if (image.isNull()) return;
  QImage img = image;
  if (std::max(img.width(), img.height()) > 4096)
    img = img.scaled(4096, 4096, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  terrainImage_ = img.convertToFormat(QImage::Format_RGBA8888);
  useTexture_ = true;
  rebuild();
}

void Scene3DRenderer::ClearTerrainTexture() {
  terrainImage_ = QImage();
  useTexture_ = false;
  rebuild();
}

void Scene3DRenderer::SetTextureFlipV(bool flip) {
  if (flipV_ == flip) return;
  flipV_ = flip;
  rebuild();
}

void Scene3DRenderer::updateClipPlanes() {
  if (!camera_) return;
  const float dist = camera_->position().distanceToPoint(camera_->viewCenter());
  const double relief =
      std::max(1.0, (heightMax_ - heightMin_) * exaggeration_);
  const double diag =
      std::sqrt(width_ * width_ + depth_ * depth_ + relief * relief);

  const float nearPlane = std::max(0.05f, dist * 0.0005f);
  const float farPlane = dist * 4.0f + float(diag) * 3.0f;

  float aspect = 16.0f / 9.0f;
  if (container_ && container_->height() > 0)
    aspect = float(container_->width()) / float(container_->height());

  camera_->lens()->setPerspectiveProjection(45.0f, aspect, nearPlane, farPlane);
}

void Scene3DRenderer::frameCamera() {
  const double relief =
      std::max(1.0, (heightMax_ - heightMin_) * exaggeration_);
  const float dist = float(std::max(width_, depth_)) * 1.25f;
  const float hOff = float(relief) * 0.9f + dist * 0.55f;

  camera_->setViewCenter(QVector3D(0, float(relief) * 0.35f, 0));
  camera_->setPosition(QVector3D(0, hOff, dist));
  camera_->setUpVector(QVector3D(0, 1, 0));
  updateClipPlanes();
}

bool Scene3DRenderer::eventFilter(QObject* obj, QEvent* ev) {
  if (customZoom_ && obj == view_ && ev->type() == QEvent::Wheel && camera_) {
    auto* we = static_cast<QWheelEvent*>(ev);

    const QVector3D center = camera_->viewCenter();
    QVector3D offset = camera_->position() - center;
    const float dist = offset.length();
    if (dist < 1e-3f) return true;

    // 支持多格滚动；向上滚（angleDelta.y > 0）= 放大 = 靠近
    const float notches =
        std::clamp(we->angleDelta().y() / 120.0f, -4.0f, 4.0f);
    const float factor = std::pow(1.0f + zoomFactor_, -notches);
    const float newDist = std::max(minDistance_, dist * factor);

    camera_->setPosition(center + offset.normalized() * newDist);
    updateClipPlanes();

    return true;  // 吞掉事件，避免 Qt3D 再按固定步长缩放一次
  }

  const bool isMouse = (ev->type() == QEvent::MouseButtonPress ||
                        ev->type() == QEvent::MouseMove ||
                        ev->type() == QEvent::MouseButtonRelease);
  if (isMouse) {
    auto* me = static_cast<QMouseEvent*>(ev);

    // container_ 与 view_ 都装了过滤器，同一事件会被投递两次；用 mouse_owner_
    // 去重
    if (ev->type() == QEvent::MouseButtonPress) {
      if (me->button() != Qt::LeftButton) return true;
      if (mouse_owner_) return true;
      mouse_owner_ = obj;
      press_pos_ = me->pos();
      potential_click_ = adding_path_;
      panning_ = !adding_path_;  // 绘点模式下先不平移
      if (panning_) lastMousePos_ = me->pos();
      return true;
    }

    if (mouse_owner_ && mouse_owner_ != obj) return true;

    if (ev->type() == QEvent::MouseMove) {
      if (!(me->buttons() & Qt::LeftButton)) return true;
      if (adding_path_ && potential_click_) {
        const QPoint d = me->pos() - press_pos_;
        if (std::abs(d.x()) > click_threshold_ ||
            std::abs(d.y()) > click_threshold_) {
          potential_click_ = false;  // 判定为拖拽 → 转为平移
          panning_ = true;
          lastMousePos_ = me->pos();
        }
      }
      if (panning_) {
        doPan(me->pos() - lastMousePos_);
        lastMousePos_ = me->pos();
      }
      return true;
    }

    // MouseButtonRelease
    if (me->button() != Qt::LeftButton) return true;
    if (adding_path_ && potential_click_) {
      QVector3D hit;
      if (PickTerrain(me->pos(), hit)) {
        AppendPathPoint(hit);
        qDebug() << "[3d-path] add point" << hit << "count"
                 << current_path_uv_.size();
      } else {
        qWarning() << "[3d-path] pick missed at" << me->pos();
      }
    }
    potential_click_ = false;
    panning_ = false;
    mouse_owner_ = nullptr;
    return true;
  }
  return QWidget::eventFilter(obj, ev);
}

void Scene3DRenderer::doPan(const QPoint& delta) {
  if (!camera_) return;

  const QVector3D center = camera_->viewCenter();
  const QVector3D pos = camera_->position();
  QVector3D viewDir = center - pos;
  const float dist = viewDir.length();
  if (dist < 1e-3f) return;
  viewDir /= dist;

  // 屏幕右方向
  QVector3D right = QVector3D::crossProduct(viewDir, camera_->upVector());
  if (right.lengthSquared() < 1e-8f) right = QVector3D(1.0f, 0.0f, 0.0f);
  right.normalize();
  // 屏幕上方向（与 right、viewDir 正交）
  const QVector3D up = QVector3D::crossProduct(right, viewDir).normalized();

  float h = view_ ? float(view_->height()) : 0.0f;
  if (h <= 1.0f && container_) h = float(container_->height());
  if (h <= 1.0f) h = 600.0f;

  const float k = dist * panFactor_ / h;  // ★ 与距离成正比

  // 拖动方向与场景移动方向相反：向右拖 → 场景右移 → 相机左移
  const QVector3D move =
      -right * (float(delta.x()) * k) + up * (float(delta.y()) * k);

  camera_->setPosition(pos + move);
  camera_->setViewCenter(center + move);
}

QVector3D Scene3DRenderer::lightDirFromAngles() const {
  const float az = azimuth_ * float(M_PI) / 180.0f;
  const float el = std::clamp(elevation_, 5.0f, 89.0f) * float(M_PI) / 180.0f;
  return QVector3D(std::sin(az) * std::cos(el), std::sin(el),
                   std::cos(az) * std::cos(el))
      .normalized();
}

void Scene3DRenderer::refreshColors() {
  qDebug() << "[light] refresh: type" << int(showType_) << "useTex"
           << useTexture_ << "hasTexImg" << (terrainTexImage_ != nullptr)
           << "amb" << ambient_ << "bri" << brightness_ << "con" << contrast_;

  if (!hasData_ || !rootEntity_) return;

  // ★ 贴图模式：改纹理图，顶点色不参与渲染
  if (useTexture_ && terrainTexImage_ && showType_ != kMap3DShowTypePoint) {
    terrainTexImage_->setImage(applyImageAdjust(terrainImage_));
    qDebug() << "[light] texture adjusted bri" << brightness_ << "con"
             << contrast_;
    return;
  }

  Qt3DCore::QEntity* ent =
      (showType_ == kMap3DShowTypePoint) ? pointCloudEntity_ : meshEntity_;
  if (!ent) return;

  // 找到实体的 QGeometryRenderer
  Qt3DRender::QGeometryRenderer* renderer = nullptr;
  for (auto* c : ent->components()) {
    if (auto* r = qobject_cast<Qt3DRender::QGeometryRenderer*>(c)) {
      renderer = r;
      break;
    }
  }
  if (!renderer || !renderer->geometry()) return;

  // 找到颜色 attribute 对应的 buffer
  Qt3DCore::QBuffer* colBuf = nullptr;
  uint colCount = 0;
  for (auto* a : renderer->geometry()->attributes()) {
    if (a->name() == Qt3DCore::QAttribute::defaultColorAttributeName()) {
      colBuf = a->buffer();
      colCount = uint(a->count());
      break;
    }
  }
  if (!colBuf || colCount == 0) return;

  // ---- 重算颜色：与 buildXxx 里的公式保持一致 ----
  const float minH = float(heightMin_), maxH = float(heightMax_);
  const float span = std::max(1e-6f, maxH - minH);
  const QVector3D L = lightDirFromAngles();

  // 判断当前是点云还是网格（点云的采样步长不同）
  const bool isPoints = (showType_ == kMap3DShowTypePoint);
  const int step = isPoints ? pointStep_ : meshStep_;
  const int sc = isPoints ? 0 : std::max(2, gridCols_ / std::max(1, step));
  const int sr = isPoints ? 0 : std::max(2, gridRows_ / std::max(1, step));
  const double dx =
      width_ / std::max(1.0, double(isPoints ? gridCols_ - 1 : sc - 1));
  const double dz =
      depth_ / std::max(1.0, double(isPoints ? gridRows_ - 1 : sr - 1));

  QByteArray colData;
  colData.resize(int(colCount * 3 * sizeof(float)));
  float* p = reinterpret_cast<float*>(colData.data());

  size_t k = 0;
  if (isPoints) {
    for (int r = 0; r < gridRows_ && k < colCount; r += step)
      for (int c = 0; c < gridCols_ && k < colCount; c += step) {
        const float hv = at(grid_, gridCols_, gridRows_, c, r);
        QVector3D col = terrainColor((hv - minH) / span);
        if (hillshade_)
          col =
              shadeColor(col,
                         gridNormal(grid_, gridCols_, gridRows_, c, r, step,
                                    float(dx), float(dz), minH, exaggeration_),
                         L, ambient_);
        col = applyBrightnessContrast(col);
        *p++ = col.x();
        *p++ = col.y();
        *p++ = col.z();
        ++k;
      }
  } else {
    // 网格：从 normal buffer 读回已算好的法线
    Qt3DCore::QBuffer* nrmBuf = nullptr;
    for (auto* a : renderer->geometry()->attributes()) {
      if (a->name() == Qt3DCore::QAttribute::defaultNormalAttributeName()) {
        nrmBuf = a->buffer();
        break;
      }
    }
    if (!nrmBuf) return;

    const QByteArray nrmData = nrmBuf->data();
    const float* np = reinterpret_cast<const float*>(nrmData.constData());
    if (nrmData.size() < int(colCount * 3 * sizeof(float))) return;

    for (uint i = 0; i < colCount; ++i) {
      // 从 grid_ 取高程决定基础色；索引 i 对应网格顶点 (rr, cc)
      const int rr = int(i) / sc, cc = int(i) % sc;
      const int c = std::min(cc * step, gridCols_ - 1);
      const int r = std::min(rr * step, gridRows_ - 1);
      const float hv = at(grid_, gridCols_, gridRows_, c, r);

      QVector3D col = terrainColor((hv - minH) / span);
      QVector3D n(np[i * 3], np[i * 3 + 1], np[i * 3 + 2]);
      if (hillshade_) col = shadeColor(col, n.normalized(), L, ambient_);
      col = applyBrightnessContrast(col);
      *p++ = col.x();
      *p++ = col.y();
      *p++ = col.z();
    }
  }

  colBuf->setData(colData);
}

QVector3D Scene3DRenderer::applyBrightnessContrast(const QVector3D& c) const {
  auto f = [this](float v) {
    v = (v - 0.5f) * contrast_ + 0.5f;  // 对比度：绕 0.5 拉伸
    v = v * brightness_;                // 亮度：线性缩放
    return std::clamp(v, 0.0f, 1.0f);
  };
  return QVector3D(f(c.x()), f(c.y()), f(c.z()));
}

void Scene3DRenderer::SetAmbient(float v) {
  ambient_ = std::clamp(v, 0.0f, 1.0f);
  refreshColors();  // ★ 不再 rebuild
}

void Scene3DRenderer::SetShadeLight(float x, float y, float z) {
  const QVector3D L = QVector3D(x, y, z).normalized();
  elevation_ = std::asin(std::clamp(L.y(), -1.0f, 1.0f)) * 180.0f / float(M_PI);
  azimuth_ = std::atan2(L.x(), L.z()) * 180.0f / float(M_PI);
  if (azimuth_ < 0) azimuth_ += 360.0f;
  refreshColors();
}

void Scene3DRenderer::SetLightAzimuth(float deg) {
  azimuth_ = std::fmod(deg, 360.0f);
  if (azimuth_ < 0) azimuth_ += 360.0f;
  refreshColors();
}

void Scene3DRenderer::SetLightElevation(float deg) {
  elevation_ = std::clamp(deg, 5.0f, 89.0f);
  refreshColors();
}

void Scene3DRenderer::SetBrightness(float v) {
  brightness_ = std::clamp(v, 0.2f, 3.0f);
  refreshColors();
}

void Scene3DRenderer::SetContrast(float v) {
  contrast_ = std::clamp(v, 0.2f, 3.0f);
  refreshColors();
}

void Scene3DRenderer::ApplyLightPreset(int idx) {
  switch (idx) {
    case 1:  // 明亮：适合暗色底图
      ambient_ = 0.65f;
      elevation_ = 55.0f;
      azimuth_ = 315.0f;
      brightness_ = 1.25f;
      contrast_ = 0.9f;
      break;
    case 2:  // 强立体：起伏明显
      ambient_ = 0.22f;
      elevation_ = 32.0f;
      azimuth_ = 315.0f;
      brightness_ = 1.1f;
      contrast_ = 1.3f;
      break;
    case 3:  // 柔和：看整体形态
      ambient_ = 0.5f;
      elevation_ = 60.0f;
      azimuth_ = 315.0f;
      brightness_ = 1.05f;
      contrast_ = 0.95f;
      break;
    default:  // 0 默认
      ambient_ = 0.35f;
      elevation_ = 45.0f;
      azimuth_ = 315.0f;
      brightness_ = 1.0f;
      contrast_ = 1.0f;
      break;
  }
  refreshColors();
  qDebug() << "[light] preset" << idx << "amb" << ambient_ << "el" << elevation_
           << "bri" << brightness_ << "con" << contrast_;
}

QImage Scene3DRenderer::applyImageAdjust(const QImage& src) const {
  if (src.isNull() || (brightness_ == 1.0f && contrast_ == 1.0f)) return src;

  // 用 256 级 LUT 快速映射，避免逐像素浮点运算
  uchar lut[256];
  for (int i = 0; i < 256; ++i) {
    float v = (i / 255.0f - 0.5f) * contrast_ + 0.5f;
    v *= brightness_;
    lut[i] = uchar(std::clamp(v, 0.0f, 1.0f) * 255.0f);
  }

  QImage out = src.convertToFormat(QImage::Format_RGBA8888);
  const int h = out.height();
  for (int y = 0; y < h; ++y) {
    uchar* line = out.scanLine(y);
    const int w4 = out.width() * 4;
    for (int x = 0; x < w4; x += 4) {
      line[x] = lut[line[x]];
      line[x + 1] = lut[line[x + 1]];
      line[x + 2] = lut[line[x + 2]];
      // alpha 不动
    }
  }
  return out;
}

// ================= 坐标换算 =================

QVector2D Scene3DRenderer::WorldToGridUV(const QVector3D& w) const {
  if (width_ <= 0.0 || depth_ <= 0.0) return QVector2D(0.5f, 0.5f);
  return QVector2D(float((double(w.x()) + width_ * 0.5) / width_),
                   float((double(w.z()) + depth_ * 0.5) / depth_));
}

float Scene3DRenderer::HeightAtUV(float u, float v) const {
  if (!hasData_ || gridCols_ < 2 || gridRows_ < 2) return 0.0f;
  u = std::clamp(u, 0.0f, 1.0f);
  v = std::clamp(v, 0.0f, 1.0f);
  const float fx = u * float(gridCols_ - 1);
  const float fy = v * float(gridRows_ - 1);
  const int x0 = int(fx), y0 = int(fy);
  const int x1 = std::min(x0 + 1, gridCols_ - 1);
  const int y1 = std::min(y0 + 1, gridRows_ - 1);
  const float tx = fx - float(x0), ty = fy - float(y0);

  const float a = grid_[size_t(y0) * gridCols_ + x0];
  const float b = grid_[size_t(y0) * gridCols_ + x1];
  const float c = grid_[size_t(y1) * gridCols_ + x0];
  const float d = grid_[size_t(y1) * gridCols_ + x1];
  const float hh =
      (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
  return float((hh - heightMin_) * exaggeration_);
}

float Scene3DRenderer::HeightAtWorld(const QVector3D& w) const {
  const QVector2D uv = WorldToGridUV(w);
  return HeightAtUV(uv.x(), uv.y());
}

QVector3D Scene3DRenderer::GridUVToWorld(const QVector2D& uv) const {
  const float x = float((double(uv.x()) - 0.5) * width_);
  const float z = float((double(uv.y()) - 0.5) * depth_);
  // 抬升一点，避免与地形 z-fighting
  const float lift = float(std::max(width_, depth_) * 0.0025);
  return QVector3D(x, HeightAtUV(uv.x(), uv.y()) + lift, z);
}

bool Scene3DRenderer::WorldToLatLon(const QVector3D& w, double& outLat,
                                    double& outLon) const {
  constexpr double kR = 6378137.0;
  const double dNorth = -double(w.z());  // 世界 +Z 指向南（网格行号向南增大）
  const double dEast = double(w.x());
  const double lat = geo_lat_ + (dNorth / kR) * 180.0 / M_PI;
  const double cosL = std::max(0.05, std::cos(lat * M_PI / 180.0));
  outLat = lat;
  outLon = geo_lon_ + (dEast / (kR * cosL)) * 180.0 / M_PI;
  return true;
}

// ================= 拾取 =================

QVector3D Scene3DRenderer::Unproject(const QMatrix4x4& invVP, float nx,
                                     float ny, float nz) {
  QVector4D p(nx, ny, nz, 1.0f);
  const QVector4D w = invVP * p;
  if (std::abs(w.w()) < 1e-6f) return QVector3D();
  return QVector3D(w.x() / w.w(), w.y() / w.w(), w.z() / w.w());
}

bool Scene3DRenderer::PickTerrain(const QPoint& localPos,
                                  QVector3D& out) const {
  if (!camera_ || !hasData_ || width_ <= 0.0 || depth_ <= 0.0) return false;

  int W = container_ ? container_->width() : (view_ ? view_->width() : 0);
  int H = container_ ? container_->height() : (view_ ? view_->height() : 0);
  if (W <= 0 || H <= 0) return false;

  const float ndcX = 2.0f * float(localPos.x()) / float(W) - 1.0f;
  const float ndcY = 1.0f - 2.0f * float(localPos.y()) / float(H);

  QMatrix4x4 invVP =
      (camera_->projectionMatrix() * camera_->viewMatrix()).inverted();
  const QVector3D p0 = Unproject(invVP, ndcX, ndcY, -1.0f);  // near
  const QVector3D p1 = Unproject(invVP, ndcX, ndcY, 1.0f);   // far
  QVector3D dir = p1 - p0;
  const float rayLen = dir.length();
  if (rayLen < 1e-6f) return false;
  dir /= rayLen;

  // ---- 射线与地形 AABB 求交，缩小步进区间 ----
  const float hx = float(width_ * 0.5), hz = float(depth_ * 0.5);
  const float yHi =
      float((heightMax_ - heightMin_) * exaggeration_) + hx * 0.01f;
  float t0 = 0.0f, t1 = rayLen;

  auto slab = [](float o, float d, float lo, float hi, float& a,
                 float& b) -> bool {
    if (std::abs(d) < 1e-9f) return (o >= lo && o <= hi);
    float ta = (lo - o) / d, tb = (hi - o) / d;
    if (ta > tb) std::swap(ta, tb);
    a = std::max(a, ta);
    b = std::min(b, tb);
    return a <= b;
  };
  if (!slab(p0.x(), dir.x(), -hx, hx, t0, t1)) return false;
  if (!slab(p0.z(), dir.z(), -hz, hz, t0, t1)) return false;
  if (!slab(p0.y(), dir.y(), 0.0f, yHi, t0, t1)) return false;
  if (t1 <= t0) return false;

  // ---- 步进找穿透点，再二分细化 ----
  const int kSteps = 256;
  auto diffAt = [&](float t) {
    const QVector3D p = p0 + dir * t;
    return p.y() - HeightAtWorld(p);
  };
  float prevT = t0;
  float prevD = diffAt(t0);
  for (int i = 1; i <= kSteps; ++i) {
    const float t = t0 + (t1 - t0) * float(i) / float(kSteps);
    const float d = diffAt(t);
    if (prevD > 0.0f && d <= 0.0f) {  // 由地上穿到地下
      float a = prevT, b = t;
      for (int k = 0; k < 24; ++k) {
        const float m = 0.5f * (a + b);
        if (diffAt(m) > 0.0f)
          a = m;
        else
          b = m;
      }
      out = p0 + dir * (0.5f * (a + b));
      return true;
    }
    prevT = t;
    prevD = d;
  }
  return false;  // 点到天空/地形外
}

// ================= 路径实体 =================

float Scene3DRenderer::PathHalfWidth() const {
  const double maxDim = std::max(width_, depth_);
  double m = path_width_m_;
  if (m <= 0.0) m = double(path_width_px_) * MetersPerWidthUnit();

  // 下限保护：极小值会让路带退化成一条线、拾取球也看不见
  const double minM = std::max(0.05, maxDim * 0.0002);
  return float(std::max(m, minM) * 0.5);
}

Qt3DCore::QEntity* Scene3DRenderer::BuildPathEntity(
    const QVector<QVector3D>& pts) {
  if (pts.isEmpty() || !rootEntity_) return nullptr;

  auto* ent = new Qt3DCore::QEntity(rootEntity_);
  const QVector3D c(path_color_.redF(), path_color_.greenF(),
                    path_color_.blueF());
  const float halfW = PathHalfWidth();

  // ---- 1) 贴地路带：沿路径左右各偏移 halfW，构成三角带 ----
  if (pts.size() >= 2) {
    const int n = pts.size();
    std::vector<QVector3D> pos, clr;
    std::vector<unsigned int> idx;
    pos.reserve(size_t(n) * 2);
    clr.reserve(size_t(n) * 2);

    QVector<QVector3D> L(n), R(n);
    for (int i = 0; i < n; ++i) {
      QVector3D d;
      if (i == 0)
        d = pts[1] - pts[0];
      else if (i == n - 1)
        d = pts[n - 1] - pts[n - 2];
      else
        d = pts[i + 1] - pts[i - 1];
      d.setY(0.0f);
      if (d.lengthSquared() < 1e-12f) d = QVector3D(1.0f, 0.0f, 0.0f);
      d.normalize();
      const QVector3D nx(-d.z(), 0.0f, d.x());  // XZ 平面内的横向
      L[i] = pts[i] + nx * halfW;
      R[i] = pts[i] - nx * halfW;
    }
    for (int i = 0; i < n; ++i) {
      pos.push_back(L[i]);
      clr.push_back(c);
      pos.push_back(R[i]);
      clr.push_back(c);
    }
    for (int i = 0; i < n - 1; ++i) {
      const unsigned a = unsigned(i * 2), b = a + 1;
      const unsigned e = unsigned((i + 1) * 2), f = e + 1;
      idx.insert(idx.end(), {a, b, e, b, f, e});
    }

    auto* rend = new Qt3DRender::QGeometryRenderer(ent);
    auto* geom = new Qt3DCore::QGeometry(rend);

    auto addAttr = [geom](Qt3DCore::QBuffer* buf, const QString& name, uint vs,
                          uint cnt) {
      auto* a = new Qt3DCore::QAttribute(geom);
      a->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
      a->setName(name);
      a->setVertexBaseType(Qt3DCore::QAttribute::Float);
      a->setVertexSize(vs);
      a->setByteStride(0);
      a->setCount(cnt);
      a->setBuffer(buf);
      return a;
    };

    auto* posBuf = new Qt3DCore::QBuffer(geom);
    posBuf->setData(packVec3(pos));
    auto* colBuf = new Qt3DCore::QBuffer(geom);
    colBuf->setData(packVec3(clr));
    geom->addAttribute(
        addAttr(posBuf, Qt3DCore::QAttribute::defaultPositionAttributeName(), 3,
                uint(pos.size())));
    geom->addAttribute(
        addAttr(colBuf, Qt3DCore::QAttribute::defaultColorAttributeName(), 3,
                uint(clr.size())));

    auto* idxBuf = new Qt3DCore::QBuffer(geom);
    QByteArray ib;
    ib.resize(int(idx.size() * sizeof(unsigned int)));
    std::copy(idx.begin(), idx.end(),
              reinterpret_cast<unsigned int*>(ib.data()));
    idxBuf->setData(ib);
    auto* ia = new Qt3DCore::QAttribute(geom);
    ia->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
    ia->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
    ia->setVertexSize(1);
    ia->setByteStride(0);
    ia->setCount(uint(idx.size()));
    ia->setBuffer(idxBuf);
    geom->addAttribute(ia);

    rend->setGeometry(geom);
    rend->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
    rend->setVertexCount(int(idx.size()));

    auto* mat = new Qt3DExtras::QPerVertexColorMaterial(ent);
    if (auto* eff = mat->effect())  // 关背面剔除，正反面都能看见
      for (auto* tech : eff->techniques())
        for (auto* pass : tech->renderPasses()) {
          auto* cf = new Qt3DRender::QCullFace(pass);
          cf->setMode(Qt3DRender::QCullFace::NoCulling);
          pass->addRenderState(cf);
        }
    ent->addComponent(rend);
    ent->addComponent(mat);
  }

  // ---- 2) 顶点球标（共享 mesh + material）----
  auto* sphere = new Qt3DExtras::QSphereMesh(ent);
  sphere->setRings(8);
  sphere->setSlices(12);
  sphere->setRadius(
      std::max(halfW * 0.9f, float(std::max(width_, depth_) * 0.0015)));
  auto* phong = new Qt3DExtras::QPhongMaterial(ent);
  phong->setAmbient(path_color_);
  phong->setDiffuse(QColor(20, 20, 20));
  phong->setSpecular(QColor(255, 255, 255));
  phong->setShininess(20.0f);

  for (const QVector3D& p : pts) {
    auto* node = new Qt3DCore::QEntity(ent);
    auto* tr = new Qt3DCore::QTransform(node);
    tr->setTranslation(p);
    node->addComponent(sphere);
    node->addComponent(phong);
    node->addComponent(tr);
  }
  return ent;
}

// ================= 路径 API =================

void Scene3DRenderer::SetAddingPath(bool enable) {
  adding_path_ = enable;
  potential_click_ = false;
  mouse_owner_ = nullptr;
  if (container_)
    container_->setCursor(enable ? Qt::CrossCursor : Qt::ArrowCursor);
  if (enable) {  // 与 2D 一致：进入绘点模式时丢弃未保存路径
    destroyEntity(current_path_entity_);
    current_path_uv_.clear();
  }
}

void Scene3DRenderer::AppendPathPoint(const QVector3D& world) {
  current_path_uv_.append(WorldToGridUV(world));
  RebuildCurrentPathEntity();
  emit SignalPathPointAdded(current_path_uv_.size());
}

void Scene3DRenderer::RebuildCurrentPathEntity() {
  destroyEntity(current_path_entity_);
  if (current_path_uv_.isEmpty()) return;
  QVector<QVector3D> pts;
  pts.reserve(current_path_uv_.size());
  for (const QVector2D& uv : current_path_uv_) pts.push_back(GridUVToWorld(uv));
  current_path_entity_ = BuildPathEntity(pts);
}

void Scene3DRenderer::RefreshAllPathEntities() {
  if (!rootEntity_) return;
  for (int i = 0; i < saved_path_entities_.size(); ++i) {
    destroyEntity(saved_path_entities_[i]);
    QVector<QVector3D> pts;
    pts.reserve(saved_paths_uv_[i].size());
    for (const QVector2D& uv : saved_paths_uv_[i])
      pts.push_back(GridUVToWorld(uv));
    saved_path_entities_[i] = BuildPathEntity(pts);
  }
  RebuildCurrentPathEntity();
}

quint64 Scene3DRenderer::SaveCurrentPath(const QString& name) {
  if (current_path_uv_.isEmpty()) return 0;

  const quint64 id = next_path_id_++;
  const double len = ComputePathLengthMeters(current_path_uv_);

  saved_paths_uv_.append(current_path_uv_);
  saved_path_ids_.append(id);
  saved_path_names_.append(name.isEmpty() ? QString("Path %1").arg(id) : name);
  saved_path_lengths_m_.append(len);
  saved_path_entities_.append(current_path_entity_);  // 直接接管已建好的实体
  current_path_entity_ = nullptr;
  current_path_uv_.clear();
  emit SignalPathSaved(id, saved_path_names_.last(), len);
  return id;
}

void Scene3DRenderer::RemoveSavedPathById(quint64 id) {
  const int idx = saved_path_ids_.indexOf(id);
  if (idx < 0) return;
  destroyEntity(saved_path_entities_[idx]);
  saved_paths_uv_.removeAt(idx);
  saved_path_ids_.removeAt(idx);
  saved_path_names_.removeAt(idx);
  saved_path_entities_.removeAt(idx);
  saved_path_lengths_m_.removeAt(idx);
  emit SignalPathRemoved(id);
}

void Scene3DRenderer::ClearAllPaths() {
  destroyEntity(current_path_entity_);
  current_path_uv_.clear();
  for (auto*& e : saved_path_entities_) destroyEntity(e);
  saved_path_entities_.clear();
  saved_paths_uv_.clear();
  saved_path_ids_.clear();
  saved_path_names_.clear();
  saved_path_lengths_m_.clear();
}

void Scene3DRenderer::SetPathColor(const QColor& c) {
  path_color_ = c;
  RefreshAllPathEntities();
}

void Scene3DRenderer::SetPathWidth(double meters) {
  path_width_m_ = meters;
  RefreshAllPathEntities();
}

QVector<QVector<QVector3D>> Scene3DRenderer::SavedPathsWorld() const {
  QVector<QVector<QVector3D>> out;
  out.reserve(saved_paths_uv_.size());
  for (const auto& uvList : saved_paths_uv_) {
    QVector<QVector3D> pts;
    pts.reserve(uvList.size());
    for (const QVector2D& uv : uvList) pts.push_back(GridUVToWorld(uv));
    out.push_back(pts);
  }
  return out;
}

void Scene3DRenderer::SetPathStyle(int width, const QColor& color) {
  path_width_px_ = std::max(1, width);
  path_color_ = color;
  // 像素线宽换算成世界米数（路带是真实几何，必须落在米单位上）
  path_width_m_ = double(path_width_px_) * MetersPerWidthUnit();

  // 只重建"正在画"的路径；之后新建的点在 BuildPathEntity 里自动用新样式
  RebuildCurrentPathEntity();
}

void Scene3DRenderer::SetAllPathsStyle(int width, const QColor& color) {
  path_width_px_ = std::max(1, width);
  path_color_ = color;
  path_width_m_ = double(path_width_px_) * MetersPerWidthUnit();

  // 已保存路径 + 当前路径一起重建（内部已含 RebuildCurrentPathEntity）
  RefreshAllPathEntities();
}

double Scene3DRenderer::MetersPerWidthUnit() const {
  const double maxDim = std::max(width_, depth_);
  if (maxDim <= 0.0) return 1.0;

  // 与 frameCamera() 保持一致：dist = maxDim * 1.25，FOV 45°
  // → 取景时视口纵向覆盖的世界高度 ≈ 2 * dist * tan(22.5°)
  int h = container_ ? container_->height() : (view_ ? view_->height() : 0);
  if (h <= 1) h = 600;  // 与 doPan() 同一兜底值
  const double worldH = 2.0 * (maxDim * 1.25) * std::tan(22.5 * M_PI / 180.0);
  return worldH / double(h);
}

double Scene3DRenderer::ComputePathLengthMeters(
    const QVector<QVector2D>& uv) const {
  if (uv.size() < 2) return 0.0;

  double total = 0.0;
  QVector3D prev = GridUVToWorld(uv.first());
  for (int i = 1; i < uv.size(); ++i) {
    const QVector3D cur = GridUVToWorld(uv.at(i));
    // 平面距离：只用 X/Z，天然不受 exaggeration_ 影响
    const double dx = double(cur.x() - prev.x());
    const double dz = double(cur.z() - prev.z());
    // 真实高差 = 渲染高度 / 垂直夸张系数
    const double dh =
        double(cur.y() - prev.y()) / std::max(0.05, double(exaggeration_));
    total += std::sqrt(dx * dx + dz * dz + dh * dh);
    prev = cur;
  }
  return total;
}