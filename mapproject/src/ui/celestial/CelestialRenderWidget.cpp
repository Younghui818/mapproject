#include "CelestialRenderWidget.h"
#include <QVBoxLayout>
#include <QVector3D>
#include <QFileInfo>
#include <QImage>
#include <QDir>

#include <QQuaternion>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointLight>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DRender/QClearBuffers>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QDiffuseSpecularMaterial>
#include <Qt3DExtras/QTextureMaterial>
#include <Qt3DRender/QTextureImage>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImageDataGenerator>
#include <Qt3DRender/QTextureImageData>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QEffect>

#include "CameraController.h"

const PlanetConfig PLANET_DATA[] = {
    {"Mercury", 0.4f, 6.0f, 0.03f, "qrc:/image/2k_mercury.jpg", QColor(169, 169, 169)},
    {"Venus", 0.9f, 9.0f, 177.4f, "qrc:/image/2k_venus.jpg", QColor(255, 198, 73)},
    {"Earth", 1.0f, 13.0f, 23.44f, "qrc:/image/2k_earth.jpg", QColor(30, 100, 255), 0.27f, 2.6f, 8.0f, 0.0f, 0.0f},
    {"Mars", 0.5f, 17.0f, 25.19f, "qrc:/image/2k_mars.jpg", QColor(255, 69, 0)},
    {"Jupiter", 2.5f, 23.0f, 3.13f, "qrc:/image/2k_jupiter.jpg", QColor(255, 165, 0)},
    {"Saturn", 2.2f, 30.0f, 26.73f, "qrc:/image/2k_saturn.jpg", QColor(238, 216, 174), 0.0f, 0.0f, 0.0f, 2.9f, 5.0f},
    {"Uranus", 1.5f, 36.0f, 97.77f, "qrc:/image/2k_uranus.jpg", QColor(173, 216, 230)},
    {"Neptune", 1.4f, 42.0f, 28.32f, "qrc:/image/2k_neptune.jpg", QColor(0, 0, 255)},
};

// 放在 CelestialRenderWidget.cpp 顶部
static QString qrcTex(const QString& fileName) {
  static QHash<QString, QString> cache;
  if (cache.contains(fileName)) return cache[fileName];

  // 从 qrc 读
  QFile src(":/image/" + fileName);
  QString tmp = QDir::temp().filePath(fileName);
  if (src.open(QIODevice::ReadOnly)) {
    QFile dst(tmp);
    if (dst.open(QIODevice::WriteOnly)) dst.write(src.readAll());
    dst.close();
    src.close();
  }
  cache[fileName] = tmp;
  return tmp;
}

namespace {

// 把 POD 数组拷成 QByteArray（QByteArray 构造会复制，安全）
QByteArray ToBytes(const void* ptr, qsizetype bytes) { return QByteArray(reinterpret_cast<const char*>(ptr), bytes); }

// 创建一个顶点属性：独立 buffer、紧密排列、float 类型
Qt3DCore::QAttribute* MakeVertexAttr(Qt3DCore::QNode* parent, const QString& name, const QByteArray& data,
                                     uint vertexSize, uint count) {
  auto* buf = new Qt3DCore::QBuffer(parent);
  buf->setData(data);
  buf->setUsage(Qt3DCore::QBuffer::StaticDraw);

  auto* attr = new Qt3DCore::QAttribute(parent);
  attr->setName(name);
  attr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
  attr->setVertexBaseType(Qt3DCore::QAttribute::Float);
  attr->setVertexSize(vertexSize);
  attr->setByteOffset(0);
  attr->setByteStride(vertexSize * sizeof(float));  // 独立 buffer，步长就是本属性大小
  attr->setCount(count);
  attr->setBuffer(buf);
  return attr;
}

}  // namespace

CelestialRenderWidget::CelestialRenderWidget(QWidget* parent) : QWidget(parent) {
  InitUI();
  Init3DScene();
}

void CelestialRenderWidget::InitUI() {
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  view_ = new Qt3DExtras::Qt3DWindow();
  layout->addWidget(QWidget::createWindowContainer(view_));
}

void CelestialRenderWidget::Init3DScene() {
  root_entity_ = new Qt3DCore::QEntity();

  // 设置背景颜色为黑色
  static_cast<Qt3DExtras::QForwardRenderer*>(view_->defaultFrameGraph())->setClearColor(Qt::black);

  // 相机
  Qt3DRender::QCamera* camera = view_->camera();
  const float aspect = static_cast<float>(width()) / static_cast<float>(std::max(1, height()));
  camera->lens()->setPerspectiveProjection(45.0f, aspect, 0.5f, 2000.0f);
  camera->setUpVector(QVector3D(0, 1, 0));
  camera->setPosition(QVector3D(0, 25.0f, 55.0f));
  camera->setViewCenter(QVector3D(0, 0, 0));

  CameraController* camera_ctrl_ = new CameraController(camera, view_, this);
  camera_ctrl_->SetZoomFactor(1.3f);
  camera_ctrl_->SetPanSpeed(0.2);  // 归一化后不再需要 0.1 这种补偿值
  camera_ctrl_->SetRotateSpeed(0.5f);

  // 光源（点光源，位于太阳中心）
  Qt3DCore::QEntity* lightEntity = new Qt3DCore::QEntity(root_entity_);
  Qt3DRender::QPointLight* light = new Qt3DRender::QPointLight(lightEntity);
  light->setColor("white");
  light->setIntensity(3.0f);
  lightEntity->addComponent(light);
  Qt3DCore::QTransform* lightTrans = new Qt3DCore::QTransform(lightEntity);
  lightTrans->setTranslation(QVector3D(0, 0, 0));
  lightEntity->addComponent(lightTrans);

  // 创建天体
  sun_entity_ = CreateSun(root_entity_);

  // 批量创建行星
  for (const auto& config : PLANET_DATA) {
    planets_[config.name] = CreatePlanet(config, root_entity_);
  }

  view_->setRootEntity(root_entity_);

  animation_spin_timer_ = new QTimer(this);
  connect(animation_spin_timer_, &QTimer::timeout, this, &CelestialRenderWidget::SlotUpdateSpinAnimation);
  animation_orbit_timer_ = new QTimer(this);
  connect(animation_orbit_timer_, &QTimer::timeout, this, &CelestialRenderWidget::SlotUpdateOrbitAnimation);
}

Qt3DCore::QEntity* CelestialRenderWidget::CreateSun(Qt3DCore::QEntity* root) {
  auto* sun = new Qt3DCore::QEntity(root);

  auto* mesh = new Qt3DExtras::QSphereMesh(sun);
  mesh->setRadius(3.0f);
  mesh->setRings(64);
  mesh->setSlices(64);
  mesh->setGenerateTangents(true);

  auto* tex = new Qt3DRender::QTexture2D(sun);
  tex->setGenerateMipMaps(true);
  tex->setMagnificationFilter(Qt3DRender::QTexture2D::Linear);
  tex->setMinificationFilter(Qt3DRender::QTexture2D::LinearMipMapLinear);

  auto* texImg = new Qt3DRender::QTextureImage(tex);
  texImg->setSource(QUrl::fromLocalFile(qrcTex("8k_sun.jpg")));
  tex->addTextureImage(texImg);

  auto* mat = new Qt3DExtras::QTextureMaterial(sun);
  mat->setTexture(tex);

  sun->addComponent(mesh);
  sun->addComponent(mat);

  qDebug() << "Diffuse property:" << mat->property("diffuse");

  return sun;
}

PlanetState CelestialRenderWidget::CreatePlanet(const PlanetConfig& config, Qt3DCore::QEntity* root) {
  PlanetState state;

  // ===== 1. 公转轴心（绕世界Y旋转）=====
  state.pivot_entity = new Qt3DCore::QEntity(root);
  state.pivot_transform = new Qt3DCore::QTransform(state.pivot_entity);
  state.pivot_entity->addComponent(state.pivot_transform);

  // ===== 2. 位置节点：把行星放到轨道半径处（不受倾角影响）=====
  // 新增一个中间节点专门负责平移，这样 tilt 就不会改变行星的世界位置
  Qt3DCore::QEntity* pos_entity = new Qt3DCore::QEntity(state.pivot_entity);
  Qt3DCore::QTransform* pos_transform = new Qt3DCore::QTransform(pos_entity);
  pos_transform->setTranslation(QVector3D(config.orbitRadius, 0.0f, 0.0f));
  pos_entity->addComponent(pos_transform);

  // ===== 3. 倾角节点：只倾斜自转轴，不影响位置 =====
  state.tilt_entity = new Qt3DCore::QEntity(pos_entity);
  state.tilt_transform = new Qt3DCore::QTransform(state.tilt_entity);
  state.tilt_transform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), config.axialTilt));
  state.tilt_entity->addComponent(state.tilt_transform);

  // ===== 4. 行星本体节点：挂在 tilt 下，负责自转 =====
  state.planet_entity = new Qt3DCore::QEntity(state.tilt_entity);

  // 网格
  Qt3DExtras::QSphereMesh* mesh = new Qt3DExtras::QSphereMesh(state.planet_entity);
  mesh->setRadius(config.radius);
  mesh->setRings(64);
  mesh->setSlices(64);
  mesh->setGenerateTangents(true);

  // 贴图
  auto* tex = new Qt3DRender::QTexture2D(state.planet_entity);
  tex->setGenerateMipMaps(true);
  tex->setMagnificationFilter(Qt3DRender::QTexture2D::Linear);
  tex->setMinificationFilter(Qt3DRender::QTexture2D::LinearMipMapLinear);

  auto* texImg = new Qt3DRender::QTextureImage(tex);
  texImg->setSource(QUrl::fromLocalFile(qrcTex(QFileInfo(config.texture).fileName())));
  tex->addTextureImage(texImg);

  auto* material = new Qt3DExtras::QDiffuseSpecularMaterial(state.planet_entity);
  material->setDiffuse(QVariant::fromValue<Qt3DRender::QAbstractTexture*>(tex));
  material->setAmbient(QColor(8, 8, 10));
  material->setSpecular(QColor(30, 30, 30));
  material->setShininess(8.0f);

  // 自转 Transform（现在只有旋转，没有平移，因为平移已经在 pos_entity 完成了）
  state.planet_transform = new Qt3DCore::QTransform(state.planet_entity);
  // 初始不平移，原点就是行星中心
  state.planet_entity->addComponent(mesh);
  state.planet_entity->addComponent(material);
  state.planet_entity->addComponent(state.planet_transform);

  // ===== 5. 轨道速度 / 自转速度 =====
  state.orbit_speed = 1.0f / (config.orbitRadius * 0.2f);
  state.spin_speed = 2.0f;
  state.current_angle = 0.0f;
  state.current_spin = 0.0f;

  // ===== 6. 轨道线（直接挂在 root 下，y=0，与行星位置完全重合）=====
  state.planet_orbit_entity = new Qt3DCore::QEntity(root);
  state.planet_orbit_entity->setObjectName("orbit_" + config.name);

  const int segments = 128;
  const float step = 2.0f * 3.14159265358979f / segments;

  QVector<QVector3D> positions;
  positions.reserve(segments);
  for (int i = 0; i < segments; ++i) {
    const float angle = i * step;
    positions.append({config.orbitRadius * std::cos(angle), 0.0f, config.orbitRadius * std::sin(angle)});
  }

  QByteArray posData;
  posData.resize(static_cast<int>(positions.size() * sizeof(QVector3D)));
  std::memcpy(posData.data(), positions.constData(), posData.size());

  auto* buf = new Qt3DCore::QBuffer(state.planet_orbit_entity);
  buf->setData(posData);
  buf->setUsage(Qt3DCore::QBuffer::StaticDraw);

  auto* posAttr = new Qt3DCore::QAttribute(state.planet_orbit_entity);
  posAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
  posAttr->setBuffer(buf);
  posAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
  posAttr->setVertexSize(3);
  posAttr->setByteOffset(0);
  posAttr->setByteStride(sizeof(QVector3D));
  posAttr->setCount(positions.size());
  posAttr->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());

  auto* geometry = new Qt3DCore::QGeometry(state.planet_orbit_entity);
  geometry->addAttribute(posAttr);

  auto* renderer = new Qt3DRender::QGeometryRenderer(state.planet_orbit_entity);
  renderer->setGeometry(geometry);
  renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::LineLoop);

  auto* orbitMaterial = new Qt3DExtras::QPhongMaterial(state.planet_orbit_entity);
  orbitMaterial->setDiffuse(QColor(150, 160, 180));
  orbitMaterial->setAmbient(QColor(120, 120, 130));
  orbitMaterial->setSpecular(QColor(20, 20, 20));
  orbitMaterial->setShininess(4.0f);

  state.planet_orbit_entity->addComponent(renderer);
  state.planet_orbit_entity->addComponent(orbitMaterial);

  // ===== 7. 卫星和行星环（需要适配新层级）=====
  CreatPlanetRings(config, state);

  return state;
}

void CelestialRenderWidget::CreatPlanetRings(const PlanetConfig& config, PlanetState& state) {
  if (config.moonRadius > 0.0f && config.moonOrbitRadius > 0.0f) {
    // 现在 tilt_entity 已经在行星位置，所以 anchor 不需要再平移 orbitRadius
    state.moon_anchor = new Qt3DCore::QEntity(state.tilt_entity);
    auto* anchorTrans = new Qt3DCore::QTransform(state.moon_anchor);
    anchorTrans->setTranslation(QVector3D(0.0f, 0.0f, 0.0f));  // ← 改为0
    // 抵消行星倾角，让月球轨道回到黄道面
    anchorTrans->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), -config.axialTilt + 5.14f));
    state.moon_anchor->addComponent(anchorTrans);

    state.moon_pivot = new Qt3DCore::QEntity(state.moon_anchor);
    state.moon_pivot_transform = new Qt3DCore::QTransform(state.moon_pivot);
    state.moon_pivot->addComponent(state.moon_pivot_transform);

    state.moon_entity = new Qt3DCore::QEntity(state.moon_pivot);

    auto* moonMesh = new Qt3DExtras::QSphereMesh(state.moon_entity);
    moonMesh->setRadius(config.moonRadius);
    moonMesh->setRings(32);
    moonMesh->setSlices(32);

    auto* moonMat = new Qt3DExtras::QDiffuseSpecularMaterial(state.moon_entity);
    const QString moonTexPath = qrcTex("2k_moon.jpg");
    if (QFile::exists(moonTexPath)) {
      auto* moonTex = new Qt3DRender::QTexture2D(state.moon_entity);
      moonTex->setGenerateMipMaps(true);
      auto* moonTexImg = new Qt3DRender::QTextureImage(moonTex);
      moonTexImg->setSource(QUrl::fromLocalFile(moonTexPath));
      moonTex->addTextureImage(moonTexImg);
      moonMat->setDiffuse(QVariant::fromValue<Qt3DRender::QAbstractTexture*>(moonTex));
    } else {
      moonMat->setDiffuse(QColor(180, 180, 180));
    }
    moonMat->setAmbient(QColor(12, 12, 12));
    moonMat->setSpecular(QColor(10, 10, 10));
    moonMat->setShininess(4.0f);

    state.moon_transform = new Qt3DCore::QTransform(state.moon_entity);
    state.moon_transform->setTranslation(QVector3D(config.moonOrbitRadius, 0.0f, 0.0f));

    state.moon_entity->addComponent(moonMesh);
    state.moon_entity->addComponent(moonMat);
    state.moon_entity->addComponent(state.moon_transform);

    state.moon_angle = 0.0f;
    state.moon_speed = config.moonOrbitSpeed;
    state.extra_visible_entities.append(state.moon_anchor);
  }

  if (config.ringInner > 0.0f && config.ringOuter > config.ringInner) {
    state.ring_entity = CreateRing(config.ringInner, config.ringOuter, state.tilt_entity, "2k_saturn_rings.png");
    auto* ringTrans = new Qt3DCore::QTransform(state.ring_entity);
    ringTrans->setTranslation(QVector3D(0.0f, 0.0f, 0.0f));
    state.ring_entity->addComponent(ringTrans);
    state.extra_visible_entities.append(state.ring_entity);
  }
}

Qt3DCore::QEntity* CelestialRenderWidget::CreateRing(float innerR, float outerR, Qt3DCore::QEntity* parent,
                                                     const QString& texturePath) {
  auto* ring = new Qt3DCore::QEntity(parent);
  ring->setObjectName("saturn_ring");

  const int segments = 128;
  const float dtheta = 2.0f * 3.14159265358979f / segments;

  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;
  positions.reserve(2 * (segments + 1));
  normals.reserve(2 * (segments + 1));
  uvs.reserve(2 * (segments + 1));

  for (int i = 0; i <= segments; ++i) {
    const float t = i * dtheta;
    const float c = std::cos(t), s = std::sin(t);
    positions.append({innerR * c, 0.0f, innerR * s});
    positions.append({outerR * c, 0.0f, outerR * s});
    normals.append({0.0f, 1.0f, 0.0f});
    normals.append({0.0f, 1.0f, 0.0f});
    // U: 内圈=0, 外圈=1 ; V: 沿圆周 0→1
    uvs.append({0.0f, i / static_cast<float>(segments)});
    uvs.append({1.0f, i / static_cast<float>(segments)});
  }

  const uint vcount = static_cast<uint>(positions.size());

  auto* posAttr = MakeVertexAttr(
      ring, Qt3DCore::QAttribute::defaultPositionAttributeName(),
      ToBytes(positions.constData(), positions.size() * static_cast<qsizetype>(sizeof(QVector3D))), 3, vcount);
  auto* nrmAttr = MakeVertexAttr(
      ring, Qt3DCore::QAttribute::defaultNormalAttributeName(),
      ToBytes(normals.constData(), normals.size() * static_cast<qsizetype>(sizeof(QVector3D))), 3, vcount);
  auto* uvAttr =
      MakeVertexAttr(ring, Qt3DCore::QAttribute::defaultTextureCoordinateAttributeName(),
                     ToBytes(uvs.constData(), uvs.size() * static_cast<qsizetype>(sizeof(QVector2D))), 2, vcount);

  auto* geometry = new Qt3DCore::QGeometry(ring);
  geometry->addAttribute(posAttr);
  geometry->addAttribute(nrmAttr);
  geometry->addAttribute(uvAttr);

  auto* renderer = new Qt3DRender::QGeometryRenderer(ring);
  renderer->setGeometry(geometry);
  renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::TriangleStrip);

  // ===== 关键：使用支持贴图的材质 =====
  if (!texturePath.isEmpty() && QFile::exists(qrcTex(QFileInfo(texturePath).fileName()))) {
    // 加载贴图
    auto* tex = new Qt3DRender::QTexture2D(ring);
    tex->setGenerateMipMaps(true);
    tex->setMagnificationFilter(Qt3DRender::QTexture2D::Linear);
    tex->setMinificationFilter(Qt3DRender::QTexture2D::LinearMipMapLinear);
    tex->setWrapMode(Qt3DRender::QTextureWrapMode());
    tex->wrapMode()->setX(Qt3DRender::QTextureWrapMode::Repeat);       // U 方向重复
    tex->wrapMode()->setY(Qt3DRender::QTextureWrapMode::ClampToEdge);  // V 方向不重复

    auto* texImg = new Qt3DRender::QTextureImage(tex);
    texImg->setSource(QUrl::fromLocalFile(qrcTex(QFileInfo(texturePath).fileName())));
    tex->addTextureImage(texImg);

    // 使用 QDiffuseSpecularMaterial 支持纹理
    auto* mat = new Qt3DExtras::QPhongMaterial(ring);
    mat->setDiffuse(QColor(200, 180, 150));
    mat->setAmbient(QColor(150, 135, 115));
    mat->setSpecular(QColor(30, 30, 30));
    mat->setShininess(6.0f);

    // 遍历材质内部的渲染通道，关闭面剔除
    Qt3DRender::QEffect* effect = mat->effect();
    if (effect) {
      for (Qt3DRender::QTechnique* tech : effect->techniques()) {
        for (Qt3DRender::QRenderPass* pass : tech->renderPasses()) {
          Qt3DRender::QCullFace* cullFace = new Qt3DRender::QCullFace(pass);
          cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
          pass->addRenderState(cullFace);
        }
      }
    }

    ring->addComponent(renderer);
    ring->addComponent(mat);
  } else {
    // 兜底：没贴图用纯色
    auto* mat = new Qt3DExtras::QPhongMaterial(ring);
    mat->setDiffuse(QColor(200, 180, 150));
    mat->setAmbient(QColor(150, 135, 115));
    mat->setSpecular(QColor(30, 30, 30));
    mat->setShininess(6.0f);
    ring->addComponent(renderer);
    ring->addComponent(mat);
  }

  return ring;
}

void CelestialRenderWidget::SlotUpdateSpinAnimation() {
  for (auto it = planets_.begin(); it != planets_.end(); ++it) {
    PlanetState& state = it.value();
    // 旋转 Planet 自身
    // 因为 Planet 是 Tilt 的子节点，所以它绕着自己的 Y 轴旋转时，
    // 这个 Y 轴已经被父节点 Tilt 倾斜了（黄赤交角），从而完美模拟了倾斜自转！
    state.current_spin += state.spin_speed;
    if (state.current_spin >= 360.0f) state.current_spin -= 360.0f;
    state.planet_transform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), state.current_spin));

    if (state.moon_pivot_transform && state.moon_transform) {
      // 公转
      state.moon_angle += state.moon_speed;
      state.moon_transform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), state.moon_angle));
    }
  }
}

void CelestialRenderWidget::SlotUpdateOrbitAnimation() {
  for (auto it = planets_.begin(); it != planets_.end(); ++it) {
    PlanetState& state = it.value();
    // 旋转 Pivot
    state.current_angle += state.orbit_speed;
    if (state.current_angle >= 360.0f) state.current_angle -= 360.0f;
    state.pivot_transform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), state.current_angle));

    if (state.moon_pivot_transform && state.moon_transform) {
      // 公转
      state.moon_angle += state.moon_speed;
      state.moon_pivot_transform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), state.moon_angle));
    }
  }
}

void CelestialRenderWidget::SlotStartSpinAnimation() {
  if (animation_spin_timer_) {
    if (animation_spin_timer_->isActive()) {
      animation_spin_timer_->stop();
    } else {
      animation_spin_timer_->start(15);
    }
  }
}

void CelestialRenderWidget::SlotStartOrbitAnimation() {
  if (animation_orbit_timer_) {
    if (animation_orbit_timer_->isActive()) {
      animation_orbit_timer_->stop();
    } else {
      animation_orbit_timer_->start(30);
    }
  }
}

void CelestialRenderWidget::SlotPlanetVisible(int index, bool visible) {
  if (index == -1) {
    if (sun_entity_) sun_entity_->setEnabled(visible);
  } else if (index >= 0 && index < 8) {
    const QString& planetName = PLANET_DATA[index].name;
    if (planets_.contains(planetName)) {
      // 隐藏/显示 Pivot 即可同时隐藏/显示其子节点
      planets_[planetName].pivot_entity->setEnabled(visible);
      planets_[planetName].planet_orbit_entity->setEnabled(visible);
      for (Qt3DCore::QEntity* extra : planets_[planetName].extra_visible_entities) {
        if (extra) extra->setEnabled(visible);
      }
    }
  }
}