#include "Path3DManager.h"

#include <QByteArray>
#include <QFile>
#include <QTimer>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QTechnique>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
QByteArray PackVec3(const std::vector<QVector3D>& values) {
  QByteArray data;
  data.resize(static_cast<int>(values.size() * 3 * sizeof(float)));
  float* target = reinterpret_cast<float*>(data.data());
  for (const QVector3D& value : values) {
    *target++ = value.x();
    *target++ = value.y();
    *target++ = value.z();
  }
  return data;
}
}  // namespace

Path3DManager::Path3DManager(Context context, QObject* parent) : QObject(parent), context_(std::move(context)) {}

void Path3DManager::DestroyEntity(Qt3DCore::QEntity*& entity) {
  if (!entity) return;
  Qt3DCore::QEntity* victim = entity;
  entity = nullptr;
  victim->setEnabled(false);
  for (auto* component : victim->components()) victim->removeComponent(component);
  victim->deleteLater();
}

float Path3DManager::PathHalfWidth() const {
  const double maxDim = context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  double meters = current_path_width_m_;
  if (meters <= 0.0) meters = double(current_path_width_px_) * MetersPerWidthUnit();
  return float(std::max(meters, std::max(0.05, maxDim * 0.0002)) * 0.5);
}

float Path3DManager::PathLift() const {
  const double maxDim = context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  return float(std::max(0.1, maxDim * 0.0004));
}

double Path3DManager::MetersPerWidthUnit() const {
  return context_.metersPerWidthUnit ? context_.metersPerWidthUnit() : 1.0;
}

QVector<QVector3D> Path3DManager::PathPointsToWorld(const QVector<PathRecord>& path, bool keepHeight) const {
  QVector<QVector3D> points;
  points.reserve(path.size());
  if (!context_.latLonToWorldAt) return points;
  for (const PathRecord& record : path) {
    QVector3D world = context_.latLonToWorldAt(record.lat, record.lon, record.ref_lat, record.ref_lon);
    world.setY(keepHeight ? record.height : 0.0f);
    points.append(world);
  }
  return points;
}

QVector<QVector3D> Path3DManager::ResamplePathOnTerrain(const QVector<QVector3D>& controlPoints, float lift) const {
  if (controlPoints.size() < 2 || !context_.heightAtWorld) return controlPoints;
  QVector<QVector3D> output;
  const double maxDim = context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  const float segmentLength = float(std::max(1.0, maxDim * 0.002));
  for (int i = 0; i < controlPoints.size() - 1; ++i) {
    const QVector3D a = controlPoints[i];
    const QVector3D b = controlPoints[i + 1];
    const int steps = std::max(1, int(a.distanceToPoint(b) / segmentLength));
    for (int step = 0; step <= steps; ++step) {
      const float t = float(step) / steps;
      QVector3D point = a * (1.0f - t) + b * t;
      point.setY(context_.heightAtWorld(point) + lift);
      output.append(point);
    }
  }
  return output;
}

Qt3DCore::QEntity* Path3DManager::BuildPathEntity(const QVector<QVector3D>& points) {
  if (points.isEmpty() || !context_.rootEntity) return nullptr;

  auto* entity = new Qt3DCore::QEntity();
  const QVector3D color(current_path_color_.redF(), current_path_color_.greenF(), current_path_color_.blueF());
  const float halfWidth = PathHalfWidth();

  // ============================================================
  // 只有1个点 → 只画一个球（不画线）
  // ============================================================
  if (points.size() == 1) {
    const QVector3D& p = points[0];

    auto* sphereMesh = new Qt3DExtras::QSphereMesh(entity);
    sphereMesh->setRings(8);
    sphereMesh->setSlices(12);
    const double maxDim = context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
    sphereMesh->setRadius(std::max(halfWidth * 0.9f, float(maxDim * 0.0015)));

    auto* sphereMaterial = new Qt3DExtras::QPhongMaterial(entity);
    sphereMaterial->setAmbient(current_path_color_);
    sphereMaterial->setDiffuse(QColor(20, 20, 20));
    sphereMaterial->setSpecular(QColor(255, 255, 255));
    sphereMaterial->setShininess(20.0f);

    auto* transform = new Qt3DCore::QTransform(entity);
    transform->setTranslation(p);

    entity->addComponent(sphereMesh);
    entity->addComponent(sphereMaterial);
    entity->addComponent(transform);

    entity->setParent(context_.rootEntity);
    if (context_.view) context_.view->requestUpdate();
    return entity;
  }

  // ============================================================
  // 多个点 → 只画路径线（三角带），不画球
  // ============================================================
  const int count = points.size();

  std::vector<QVector3D> positions, colors;
  std::vector<unsigned int> indices;
  positions.reserve(size_t(count) * 2);
  colors.reserve(size_t(count) * 2);

  QVector<QVector3D> left(count), right(count);
  for (int i = 0; i < count; ++i) {
    QVector3D direction;
    if (i == 0)
      direction = points[1] - points[0];
    else if (i == count - 1)
      direction = points[count - 1] - points[count - 2];
    else
      direction = points[i + 1] - points[i - 1];
    direction.setY(0.0f);
    if (direction.lengthSquared() < 1e-12f) direction = QVector3D(1.0f, 0.0f, 0.0f);
    direction.normalize();
    const QVector3D normal(-direction.z(), 0.0f, direction.x());
    left[i] = points[i] + normal * halfWidth;
    right[i] = points[i] - normal * halfWidth;
  }

  for (int i = 0; i < count; ++i) {
    positions.push_back(left[i]);
    colors.push_back(color);
    positions.push_back(right[i]);
    colors.push_back(color);
  }

  for (int i = 0; i < count - 1; ++i) {
    const unsigned int a = unsigned(i * 2), b = a + 1;
    const unsigned int c = unsigned((i + 1) * 2), d = c + 1;
    indices.insert(indices.end(), {a, b, c, b, d, c});
  }

  auto* renderer = new Qt3DRender::QGeometryRenderer(entity);
  auto* geometry = new Qt3DCore::QGeometry(renderer);

  auto addAttribute = [geometry](Qt3DCore::QBuffer* buffer, const QString& name, uint vertexSize, uint attributeCount) {
    auto* attribute = new Qt3DCore::QAttribute(geometry);
    attribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    attribute->setName(name);
    attribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
    attribute->setVertexSize(vertexSize);
    attribute->setByteStride(0);
    attribute->setCount(attributeCount);
    attribute->setBuffer(buffer);
    return attribute;
  };

  auto* positionBuffer = new Qt3DCore::QBuffer(geometry);
  positionBuffer->setData(PackVec3(positions));
  auto* colorBuffer = new Qt3DCore::QBuffer(geometry);
  colorBuffer->setData(PackVec3(colors));

  geometry->addAttribute(
      addAttribute(positionBuffer, Qt3DCore::QAttribute::defaultPositionAttributeName(), 3, uint(positions.size())));
  geometry->addAttribute(
      addAttribute(colorBuffer, Qt3DCore::QAttribute::defaultColorAttributeName(), 3, uint(colors.size())));

  auto* indexBuffer = new Qt3DCore::QBuffer(geometry);
  QByteArray indexData;
  indexData.resize(int(indices.size() * sizeof(unsigned int)));
  std::copy(indices.begin(), indices.end(), reinterpret_cast<unsigned int*>(indexData.data()));
  indexBuffer->setData(indexData);

  auto* indexAttribute = new Qt3DCore::QAttribute(geometry);
  indexAttribute->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
  indexAttribute->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
  indexAttribute->setVertexSize(1);
  indexAttribute->setByteStride(0);
  indexAttribute->setCount(uint(indices.size()));
  indexAttribute->setBuffer(indexBuffer);
  geometry->addAttribute(indexAttribute);

  renderer->setGeometry(geometry);
  renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
  renderer->setVertexCount(int(indices.size()));

  auto* lineMaterial = new Qt3DExtras::QPerVertexColorMaterial(entity);
  if (auto* effect = lineMaterial->effect())
    for (auto* technique : effect->techniques())
      for (auto* pass : technique->renderPasses()) {
        auto* cullFace = new Qt3DRender::QCullFace(pass);
        cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
        pass->addRenderState(cullFace);
      }

  entity->addComponent(renderer);
  entity->addComponent(lineMaterial);

  entity->setParent(context_.rootEntity);
  if (context_.view) context_.view->requestUpdate();
  return entity;
}

void Path3DManager::AddPoint(const PathRecord& point) {
  current_path_.points.append(point);
  emit SignalPathPointAdded();
}

PathData Path3DManager::GetCurrentPathPoints() { return current_path_; }

void Path3DManager::SetAddingPath(bool enable) {
  if (!enable) return;
  DestroyEntity(current_path_entity_);
  current_path_.points.clear();
}

void Path3DManager::RebuildCurrentPathEntity() {
  DestroyEntity(current_path_entity_);
  if (current_path_.points.isEmpty()) return;
  current_path_entity_ =
      BuildPathEntity(ResamplePathOnTerrain(PathPointsToWorld(current_path_.polyline, true), PathLift()));
}

void Path3DManager::RefreshAllPathEntities() {
  RebuildCurrentPathEntity();
  for (auto it = saved_path_entities_map_.begin(); it != saved_path_entities_map_.end(); ++it) {
    DestroyEntity(it.value());
  }

  // TODO: 传入已保存路径
  // saved_path_entities_[i] =
  // BuildPathEntity(ResamplePathOnTerrain(PathPointsToWorld(saved_paths_[i], false), PathLift()));
}

void Path3DManager::BuildPathEntities(PathData path_data) {
  if (path_data.id == current_path_.id) {
    // 如果是当前绘制路径
    current_path_ = path_data;
    RebuildCurrentPathEntity();
  } else {
    // 如果是已存在路径
    for (auto it = saved_path_entities_map_.begin(); it != saved_path_entities_map_.end(); ++it) {
      if (path_data.id == it.key()) {
        DestroyEntity(it.value());
        break;
      }
    }
    saved_path_entities_map_[path_data.id] =
        BuildPathEntity(ResamplePathOnTerrain(PathPointsToWorld(path_data.polyline, false), PathLift()));
  }
}

quint64 Path3DManager::SaveCurrentPath(const QString& name) {
  if (current_path_.points.isEmpty()) return 0;
  current_path_.id = next_path_id_++;
  current_path_.name = name.isEmpty() ? QString("Path %1").arg(current_path_.id) : name;
  saved_path_entities_map_[current_path_.id] = current_path_entity_;
  current_path_entity_ = nullptr;
  current_path_.points.clear();
  emit SignalPathSaved(current_path_);
  return current_path_.id;
}

void Path3DManager::RemoveSavedPathById(quint64 id) {
  // 已保存路径
  for (auto it = saved_path_entities_map_.begin(); it != saved_path_entities_map_.end(); ++it) {
    if (id != it.key()) {
      continue;
    }
    DestroyEntity(it.value());
    saved_path_entities_map_.remove(id);
    break;
  }
  emit SignalPathRemoved(id);
}

void Path3DManager::ClearAllPaths() {
  DestroyEntity(current_path_entity_);
  current_path_.points.clear();
  for (auto it = saved_path_entities_map_.begin(); it != saved_path_entities_map_.end(); ++it) {
    DestroyEntity(it.value());
    break;
  }
  saved_path_entities_map_.clear();

  if (path_anim_timer_) {
    path_anim_timer_->stop();
    delete path_anim_timer_;
    path_anim_timer_ = nullptr;
  }
  DestroyEntity(anim_path_entity_);
  anim_geom_renderer_ = nullptr;
  anim_pos_buffer_ = nullptr;
  anim_col_buffer_ = nullptr;
  anim_idx_buffer_ = nullptr;
  path_animating_ = false;
  anim_ctrl_pts_.clear();
}

void Path3DManager::SetPathStyle(int width, const QColor& color) {
  current_path_width_px_ = std::max(1, width);
  current_path_color_ = color;
  current_path_width_m_ = double(current_path_width_px_) * MetersPerWidthUnit();
  RebuildCurrentPathEntity();
  emit SignalPathStyleChanged();
}

void Path3DManager::SetAllPathsStyle(int width, const QColor& color) {
  SetPathStyle(width, color);
  RefreshAllPathEntities();
}

void Path3DManager::StartPathGrowAnimation(const QVector<QVector3D>& controlPoints, int savedIndex) {
  if (controlPoints.size() < 2) return;
  if (path_anim_timer_) {
    path_anim_timer_->stop();
    delete path_anim_timer_;
    path_anim_timer_ = nullptr;
  }
  if (anim_path_entity_) {
    anim_path_entity_->setEnabled(false);
    anim_path_entity_->deleteLater();
    anim_path_entity_ = nullptr;
  }
  anim_geom_renderer_ = nullptr;
  anim_pos_buffer_ = nullptr;
  anim_col_buffer_ = nullptr;
  anim_idx_buffer_ = nullptr;
  anim_ctrl_pts_ = controlPoints;
  path_anim_progress_ = 0.0f;
  path_animating_ = true;
  anim_path_id_ = savedIndex;

  if (saved_path_entities_map_.count(savedIndex) > 0) {
    DestroyEntity(saved_path_entities_map_[savedIndex]);
  }

  path_anim_timer_ = new QTimer(this);
  connect(path_anim_timer_, &QTimer::timeout, this, [this]() {
    path_anim_progress_ += 1.0f / 60.0f;
    if (path_anim_progress_ >= 1.0f) {
      path_anim_progress_ = 1.0f;
      UpdatePathGrowFrame();
      path_anim_timer_->stop();
      delete path_anim_timer_;
      path_anim_timer_ = nullptr;
      OnPathGrowFinished();
      return;
    }
    UpdatePathGrowFrame();
  });
  path_anim_timer_->start(50);
}

void Path3DManager::UpdatePathGrowFrame() {
  if (anim_ctrl_pts_.size() < 2) return;
  const int totalSegments = anim_ctrl_pts_.size() - 1;
  const float scaledProgress = path_anim_progress_ * float(totalSegments);
  const int visibleSegments = int(std::floor(scaledProgress));
  const float segmentProgress = scaledProgress - float(visibleSegments);
  if (visibleSegments < 0) return;
  QVector<QVector3D> visible;
  const int maxVisible = std::min(visibleSegments + 1, totalSegments + 1);
  for (int i = 0; i < maxVisible; ++i) visible.append(anim_ctrl_pts_[i]);
  if (visibleSegments < totalSegments) {
    const QVector3D& a = anim_ctrl_pts_[visibleSegments];
    const QVector3D& b = anim_ctrl_pts_[visibleSegments + 1];
    visible.append(a * (1.0f - segmentProgress) + b * segmentProgress);
  }
  if (visible.size() < 2) return;
  const QVector<QVector3D> groundPoints = ResamplePathOnTerrain(visible, PathLift());
  if (!anim_path_entity_) BuildAnimEntityShell();
  UpdateAnimGeometry(groundPoints);
}

void Path3DManager::OnPathGrowFinished() {
  path_animating_ = false;
  if (anim_path_entity_) {
    anim_path_entity_->setEnabled(false);
    anim_path_entity_->deleteLater();
    anim_path_entity_ = nullptr;
  }
  anim_geom_renderer_ = nullptr;
  anim_pos_buffer_ = nullptr;
  anim_col_buffer_ = nullptr;
  anim_idx_buffer_ = nullptr;
  // 重建路径
  if (saved_path_entities_map_.count(anim_path_id_) > 0) {
    DestroyEntity(saved_path_entities_map_[anim_path_id_]);
  }
  saved_path_entities_map_[anim_path_id_] =
      BuildPathEntity(ResamplePathOnTerrain(PathPointsToWorld(anim_path_point_, false), PathLift()));
  emit SignalPathAnimationFinished();
}

void Path3DManager::PlayPathAnimation(PathData path_data) {
  if (saved_path_entities_map_.count(path_data.id) <= 0) return;
  anim_path_point_ = path_data.polyline;
  StartPathGrowAnimation(PathPointsToWorld(path_data.polyline, false), path_data.id);
}

void Path3DManager::BuildAnimEntityShell() {
  if (!context_.rootEntity) return;
  const int totalSegments = anim_ctrl_pts_.size() - 1;
  const double maxDim = context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  const float segmentLength = float(std::max(1.0, maxDim * 0.002));
  float maxSegmentWorldLength = 0.0f;
  for (int i = 0; i < totalSegments; ++i)
    maxSegmentWorldLength = std::max(maxSegmentWorldLength, anim_ctrl_pts_[i].distanceToPoint(anim_ctrl_pts_[i + 1]));
  const int maxSteps = std::max(1, int(maxSegmentWorldLength / segmentLength));
  anim_max_verts_ = (totalSegments * (maxSteps + 1) + 1) * 2;
  anim_max_idx_ = totalSegments * (maxSteps + 1) * 6;

  anim_path_entity_ = new Qt3DCore::QEntity(context_.rootEntity);
  anim_geom_renderer_ = new Qt3DRender::QGeometryRenderer(anim_path_entity_);
  auto* geometry = new Qt3DCore::QGeometry(anim_geom_renderer_);
  anim_pos_buffer_ = new Qt3DCore::QBuffer(geometry);
  QByteArray positionData(anim_max_verts_ * 3 * sizeof(float), 0);
  anim_pos_buffer_->setData(positionData);
  auto* positionAttribute = new Qt3DCore::QAttribute(geometry);
  positionAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
  positionAttribute->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());
  positionAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
  positionAttribute->setVertexSize(3);
  positionAttribute->setByteStride(3 * sizeof(float));
  positionAttribute->setCount(0);
  positionAttribute->setBuffer(anim_pos_buffer_);
  geometry->addAttribute(positionAttribute);

  anim_col_buffer_ = new Qt3DCore::QBuffer(geometry);
  QByteArray colorData(anim_max_verts_ * 3 * sizeof(float), 0);
  anim_col_buffer_->setData(colorData);
  auto* colorAttribute = new Qt3DCore::QAttribute(geometry);
  colorAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
  colorAttribute->setName(Qt3DCore::QAttribute::defaultColorAttributeName());
  colorAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
  colorAttribute->setVertexSize(3);
  colorAttribute->setByteStride(3 * sizeof(float));
  colorAttribute->setCount(0);
  colorAttribute->setBuffer(anim_col_buffer_);
  geometry->addAttribute(colorAttribute);

  anim_idx_buffer_ = new Qt3DCore::QBuffer(geometry);
  QByteArray indexData(anim_max_idx_ * sizeof(unsigned int), 0);
  anim_idx_buffer_->setData(indexData);
  auto* indexAttribute = new Qt3DCore::QAttribute(geometry);
  indexAttribute->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
  indexAttribute->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
  indexAttribute->setVertexSize(1);
  indexAttribute->setByteStride(sizeof(unsigned int));
  indexAttribute->setCount(0);
  indexAttribute->setBuffer(anim_idx_buffer_);
  geometry->addAttribute(indexAttribute);
  anim_geom_renderer_->setGeometry(geometry);
  anim_geom_renderer_->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
  anim_geom_renderer_->setVertexCount(0);

  auto* material = new Qt3DExtras::QPerVertexColorMaterial(anim_path_entity_);
  if (auto* effect = material->effect())
    for (auto* technique : effect->techniques())
      for (auto* pass : technique->renderPasses()) {
        auto* cullFace = new Qt3DRender::QCullFace(pass);
        cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
        pass->addRenderState(cullFace);
      }
  anim_path_entity_->addComponent(anim_geom_renderer_);
  anim_path_entity_->addComponent(material);
}

void Path3DManager::UpdateAnimGeometry(const QVector<QVector3D>& groundPoints) {
  if (!anim_path_entity_ || groundPoints.size() < 2) return;
  const int count = groundPoints.size();
  const QVector3D color(current_path_color_.redF(), current_path_color_.greenF(), current_path_color_.blueF());
  const float halfWidth = PathHalfWidth();
  std::vector<QVector3D> positions, colors;
  std::vector<unsigned int> indices;
  positions.reserve(size_t(count) * 2);
  colors.reserve(size_t(count) * 2);
  QVector<QVector3D> left(count), right(count);
  for (int i = 0; i < count; ++i) {
    QVector3D direction;
    if (i == 0)
      direction = groundPoints[1] - groundPoints[0];
    else if (i == count - 1)
      direction = groundPoints[count - 1] - groundPoints[count - 2];
    else
      direction = groundPoints[i + 1] - groundPoints[i - 1];
    direction.setY(0.0f);
    if (direction.lengthSquared() < 1e-12f) direction = QVector3D(1.0f, 0.0f, 0.0f);
    direction.normalize();
    const QVector3D normal(-direction.z(), 0.0f, direction.x());
    left[i] = groundPoints[i] + normal * halfWidth;
    right[i] = groundPoints[i] - normal * halfWidth;
  }
  for (int i = 0; i < count; ++i) {
    positions.push_back(left[i]);
    colors.push_back(color);
    positions.push_back(right[i]);
    colors.push_back(color);
  }
  for (int i = 0; i < count - 1; ++i) {
    const unsigned int a = unsigned(i * 2), b = a + 1;
    const unsigned int c = unsigned((i + 1) * 2), d = c + 1;
    indices.insert(indices.end(), {a, b, c, b, d, c});
  }

  QByteArray positionData(int(positions.size() * 3 * sizeof(float)), 0);
  float* positionTarget = reinterpret_cast<float*>(positionData.data());
  for (size_t i = 0; i < positions.size(); ++i) {
    positionTarget[i * 3] = positions[i].x();
    positionTarget[i * 3 + 1] = positions[i].y();
    positionTarget[i * 3 + 2] = positions[i].z();
  }
  anim_pos_buffer_->setData(positionData);
  QByteArray colorData(int(colors.size() * 3 * sizeof(float)), 0);
  float* colorTarget = reinterpret_cast<float*>(colorData.data());
  for (size_t i = 0; i < colors.size(); ++i) {
    colorTarget[i * 3] = colors[i].x();
    colorTarget[i * 3 + 1] = colors[i].y();
    colorTarget[i * 3 + 2] = colors[i].z();
  }
  anim_col_buffer_->setData(colorData);
  QByteArray indexData(int(indices.size() * sizeof(unsigned int)), 0);
  std::copy(indices.begin(), indices.end(), reinterpret_cast<unsigned int*>(indexData.data()));
  anim_idx_buffer_->setData(indexData);
  for (auto* attribute : anim_geom_renderer_->geometry()->attributes())
    attribute->setCount(
        uint(attribute->attributeType() == Qt3DCore::QAttribute::IndexAttribute ? indices.size() : positions.size()));
  anim_geom_renderer_->setVertexCount(int(indices.size()));
  if (context_.view) context_.view->requestUpdate();
}