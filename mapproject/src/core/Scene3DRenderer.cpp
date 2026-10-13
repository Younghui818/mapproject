#include "Scene3DRenderer.h"

#include <QDebug>
#include <QForwardRenderer>
#include <QLabel>
#include <QOpenGLContext>
#include <QPhongMaterial>
#include <QPixmap>
#include <QPointLight>
#include <QVBoxLayout>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QGraphicsApiFilter>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QPointSize>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QRenderStateSet>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QTechnique>
#include <algorithm>
#include <cmath>

namespace {
// ---------- OpenGL 后端（QT3D_RENDERER=opengl）----------
const char* kVS_GL = R"(
#version 330 core
in vec3 vertexPosition;
in vec3 vertexColor;
uniform mat4 modelViewProjection;
uniform float pointSize;
out vec3 vColor;
void main() {
  gl_Position = modelViewProjection * vec4(vertexPosition, 1.0);
  gl_PointSize = pointSize;
  vColor = vertexColor;
})";

const char* kFS_GL = R"(
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); })";

// ---------- ANGLE / GLES ----------
const char* kVS_ES = R"(
#version 300 es
precision highp float;
in vec3 vertexPosition;
in vec3 vertexColor;
uniform mat4 modelViewProjection;
uniform float pointSize;
out vec3 vColor;
void main() {
  gl_Position = modelViewProjection * vec4(vertexPosition, 1.0);
  gl_PointSize = pointSize;
  vColor = vertexColor;
})";

const char* kFS_ES = R"(
#version 300 es
precision highp float;
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); })";

// ---------- RHI 后端（D3D11 / Vulkan / Metal）----------
const char* kVS_RHI = R"(
#version 450 core
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;
layout(location = 0) out vec3 vColor;
layout(std140, binding = 0) uniform qt3d_render_view_uniforms {
  mat4 viewMatrix; mat4 projectionMatrix; mat4 uncorrectedProjectionMatrix;
  mat4 clipCorrectionMatrix; mat4 viewProjectionMatrix; mat4 inverseViewMatrix;
  mat4 inverseProjectionMatrix; mat4 inverseViewProjectionMatrix;
  mat4 viewportMatrix; mat4 inverseViewportMatrix;
  vec4 textureTransformMatrix; vec3 eyePosition; float aspectRatio;
  float gamma; float exposure; float time; float yUpInNDC; float yUpInFBO;
};
layout(std140, binding = 1) uniform qt3d_command_uniforms {
  mat4 modelMatrix; mat4 inverseModelMatrix; mat4 modelViewMatrix;
  mat3 modelNormalMatrix; mat4 inverseModelViewMatrix;
  mat4 modelViewProjection; mat4 inverseModelViewProjectionMatrix;
};
uniform float pointSize;
void main() {
  gl_Position = modelViewProjection * vec4(vertexPosition, 1.0);
  gl_PointSize = pointSize;
  vColor = vertexColor;
})";

const char* kFS_RHI = R"(
#version 450 core
layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); })";

// 高度→彩虹色，用来肉眼确认高度映射正确
QVector3D heightRamp(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  QColor c = QColor::fromHslF((1.0f - t) * 0.66f, 1.0f, 0.5f);  // 红→蓝
  return QVector3D(c.redF(), c.greenF(), c.blueF());
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
}  // namespace

// ============================================================
Qt3DRender::QMaterial* Scene3DRenderer::createVertexColorMaterial(
    Qt3DCore::QNode* parent) {
  auto* material = new Qt3DRender::QMaterial(parent);

  pointSizeParam_ = new Qt3DRender::QParameter(material);
  pointSizeParam_->setName(QStringLiteral("pointSize"));
  pointSizeParam_->setValue(pointSize_);
  material->addParameter(pointSizeParam_);

  auto* effect = new Qt3DRender::QEffect(material);

  // --- Technique 1: OpenGL / GL 330 ---
  {
    auto* tech = new Qt3DRender::QTechnique(effect);
    tech->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    tech->graphicsApiFilter()->setProfile(
        Qt3DRender::QGraphicsApiFilter::CoreProfile);
    tech->graphicsApiFilter()->setMajorVersion(3);
    tech->graphicsApiFilter()->setMinorVersion(3);

    auto* shader = new Qt3DRender::QShaderProgram(tech);
    shader->setVertexShaderCode(kVS_GL);
    shader->setFragmentShaderCode(kFS_GL);

    auto* pass = new Qt3DRender::QRenderPass(tech);
    pass->setShaderProgram(shader);
    auto* ps = new Qt3DRender::QPointSize(tech);
    ps->setSizeMode(Qt3DRender::QPointSize::Programmable);
    pass->addRenderState(ps);
    tech->addRenderPass(pass);
    effect->addTechnique(tech);
  }

  // --- Technique 2: OpenGL ES / GLES 300 ---
  {
    auto* tech = new Qt3DRender::QTechnique(effect);
    tech->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGLES);
    tech->graphicsApiFilter()->setProfile(
        Qt3DRender::QGraphicsApiFilter::NoProfile);
    tech->graphicsApiFilter()->setMajorVersion(3);
    tech->graphicsApiFilter()->setMinorVersion(0);

    auto* shader = new Qt3DRender::QShaderProgram(tech);
    shader->setVertexShaderCode(kVS_ES);
    shader->setFragmentShaderCode(kFS_ES);

    auto* pass = new Qt3DRender::QRenderPass(tech);
    pass->setShaderProgram(shader);
    auto* ps = new Qt3DRender::QPointSize(tech);
    ps->setSizeMode(Qt3DRender::QPointSize::Programmable);
    pass->addRenderState(ps);
    tech->addRenderPass(pass);
    effect->addTechnique(tech);
  }

  // --- Technique 3: RHI / GLSL 450 ---
  {
    auto* tech = new Qt3DRender::QTechnique(effect);
    tech->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::RHI);
    tech->graphicsApiFilter()->setProfile(
        Qt3DRender::QGraphicsApiFilter::NoProfile);
    tech->graphicsApiFilter()->setMajorVersion(1);
    tech->graphicsApiFilter()->setMinorVersion(0);

    auto* shader = new Qt3DRender::QShaderProgram(tech);
    shader->setVertexShaderCode(kVS_RHI);
    shader->setFragmentShaderCode(kFS_RHI);

    auto* pass = new Qt3DRender::QRenderPass(tech);
    pass->setShaderProgram(shader);
    tech->addRenderPass(pass);
    effect->addTechnique(tech);
  }

  material->setEffect(effect);
  return material;
}
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

void Scene3DRenderer::DebugTest(int stage) {
  ClearPointCloud();

  if (stage == 1) {
    auto* e = new Qt3DCore::QEntity(rootEntity_);
    auto* mesh = new Qt3DExtras::QCuboidMesh(e);
    mesh->setXExtent(60);
    mesh->setYExtent(60);
    mesh->setZExtent(60);
    auto* mat = new Qt3DExtras::QPhongMaterial(e);
    mat->setDiffuse(QColor(255, 255, 0));
    e->addComponent(mesh);
    e->addComponent(mat);
    camera_->setViewCenter(QVector3D(0, 0, 0));
    camera_->setPosition(QVector3D(0, 80, 160));
    qDebug() << "[stage1] cube at origin";
    return;
  }

  if (stage == 2) {
    std::vector<QVector3D> pts, clr;
    for (int i = 0; i < 40; ++i)
      for (int j = 0; j < 40; ++j) {
        pts.emplace_back(i * 2.5f - 50.0f, 0.0f, j * 2.5f - 50.0f);
        clr.emplace_back(i / 40.0f, j / 40.0f, 0.8f);
      }
    QByteArray posData(pts.size() * 3 * int(sizeof(float)), Qt::Uninitialized);
    QByteArray colData(pts.size() * 3 * int(sizeof(float)), Qt::Uninitialized);
    float* p = reinterpret_cast<float*>(posData.data());
    float* q = reinterpret_cast<float*>(colData.data());
    for (size_t i = 0; i < pts.size(); ++i) {
      *p++ = pts[i].x();
      *p++ = pts[i].y();
      *p++ = pts[i].z();
      *q++ = clr[i].x();
      *q++ = clr[i].y();
      *q++ = clr[i].z();
    }
    auto* posBuf = new Qt3DCore::QBuffer();
    posBuf->setData(posData);
    auto* colBuf = new Qt3DCore::QBuffer();
    colBuf->setData(colData);
    auto* geom = new Qt3DCore::QGeometry();
    posBuf->setParent(geom);
    colBuf->setParent(geom);
    geom->addAttribute(makeVertexAttr(
        geom, posBuf, Qt3DCore::QAttribute::defaultPositionAttributeName(), 3,
        uint(pts.size())));
    geom->addAttribute(makeVertexAttr(
        geom, colBuf, Qt3DCore::QAttribute::defaultColorAttributeName(), 3,
        uint(pts.size())));

    auto* renderer = new Qt3DRender::QGeometryRenderer();
    geom->setParent(renderer);
    renderer->setGeometry(geom);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Points);
    renderer->setVertexCount(int(pts.size()));

    pointCloudEntity_ = new Qt3DCore::QEntity(rootEntity_);
    renderer->setParent(pointCloudEntity_);
    pointCloudEntity_->addComponent(renderer);
    pointCloudEntity_->addComponent(
        new Qt3DExtras::QPerVertexColorMaterial(pointCloudEntity_));

    camera_->setViewCenter(QVector3D(0, 0, 0));
    camera_->setPosition(QVector3D(0, 120, 120));
    qDebug() << "[stage2] 1600 pts, official material";
    return;
  }

  if (stage == 3) {
    SetPointSize(60.0f);
    SetPointCloud({{-30, 0, 0}, {30, 0, 0}, {0, 40, 0}},
                  {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}});
    camera_->setViewCenter(QVector3D(0, 20, 0));
    camera_->setPosition(QVector3D(0, 60, 150));
    qDebug()
        << "[stage3] 3 giant points, official material + QPointSize(Fixed)";
    return;
  }
}

void Scene3DRenderer::SetMesh(const MeshData& mesh) {}

void Scene3DRenderer::destroyEntity(Qt3DCore::QEntity*& e) {
  if (!e) return;
  Qt3DCore::QEntity* victim = e;
  const bool wasCloud = (victim == pointCloudEntity_);  // 先记，别等置空后再比
  e = nullptr;
  victim->setEnabled(false);
  victim->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
  victim->deleteLater();
  if (wasCloud) pointSizeParam_ = nullptr;
}

void Scene3DRenderer::frameCamera(double width, double depth,
                                  double maxHeight) {
  float dist = static_cast<float>(std::max(width, depth)) * 1.6f;
  float hOff = static_cast<float>(maxHeight) * 1.2f + dist * 0.4f;
  camera_->setViewCenter(QVector3D(0, static_cast<float>(maxHeight) * 0.3f, 0));
  camera_->setPosition(QVector3D(0, hOff, dist));
  camera_->setUpVector(QVector3D(0, 1, 0));
}

void Scene3DRenderer::SetPointCloud(const QImage& heightMap, double width,
                                    double depth, double maxHeight, int step) {
  if (heightMap.isNull() || step < 1) return;
  const int cols = heightMap.width(), rows = heightMap.height();

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
      clr.push_back(heightRamp(maxHeight > 0 ? h / maxHeight : 0.0f));
    }
  }
  SetPointCloud(pts, clr);
  frameCamera(width, depth, maxHeight);
}

void Scene3DRenderer::SetPointCloud(const std::vector<QVector3D>& points,
                                    const std::vector<QVector3D>& colors) {
  ClearPointCloud();
  if (points.empty()) return;
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

  // 官方材质 + 注入 QPointSize(Fixed)，绕开着色器，OpenGL 后端下必出大点
  auto* mat = new Qt3DExtras::QPerVertexColorMaterial(pointCloudEntity_);
  if (auto* eff = mat->effect()) {
    for (auto* tech : eff->techniques()) {
      for (auto* pass : tech->renderPasses()) {
        auto* ps = new Qt3DRender::QPointSize(pass);
        ps->setSizeMode(Qt3DRender::QPointSize::Fixed);
        ps->setValue(pointSize_);
        pass->addRenderState(ps);
      }
    }
  }
  pointCloudEntity_->addComponent(mat);

  qDebug() << "--- Point cloud built --- points:" << n << "size:" << pointSize_;
}

void Scene3DRenderer::SetPointSize(float px) {
  pointSize_ = px;
  if (pointSizeParam_) pointSizeParam_->setValue(px);

  // 同步已挂在官方材质上的 QPointSize(Fixed)
  if (pointCloudEntity_) {
    for (auto* comp : pointCloudEntity_->components()) {
      auto* m = qobject_cast<Qt3DRender::QMaterial*>(comp);
      if (!m || !m->effect()) continue;
      for (auto* tech : m->effect()->techniques())
        for (auto* pass : tech->renderPasses())
          for (auto* rs : pass->renderStates())
            if (auto* ps = qobject_cast<Qt3DRender::QPointSize*>(rs))
              ps->setValue(px);
    }
  }
}

void Scene3DRenderer::ClearPointCloud() { destroyEntity(pointCloudEntity_); }