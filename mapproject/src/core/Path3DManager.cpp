#include "Path3DManager.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

Path3DManager::Path3DManager(Context context, QObject* parent)
    : QObject(parent), context_(std::move(context)) {}

void Path3DManager::destroyEntity(Qt3DCore::QEntity*& entity) {
  if (!entity) return;
  Qt3DCore::QEntity* victim = entity;
  entity = nullptr;
  victim->setEnabled(false);
  for (auto* component : victim->components())
    victim->removeComponent(component);
  victim->deleteLater();
}

float Path3DManager::PathHalfWidth() const {
  const double maxDim =
      context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  double meters = path_width_m_;
  if (meters <= 0.0) meters = double(path_width_px_) * MetersPerWidthUnit();
  return float(std::max(meters, std::max(0.05, maxDim * 0.0002)) * 0.5);
}

float Path3DManager::PathLift() const {
  const double maxDim =
      context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  return float(std::max(0.1, maxDim * 0.0004));
}

double Path3DManager::MetersPerWidthUnit() const {
  return context_.metersPerWidthUnit ? context_.metersPerWidthUnit() : 1.0;
}

double Path3DManager::ComputePathLengthMeters(
    const QVector<Path3DRecord>& path) const {
  if (path.size() < 2) return 0.0;
  constexpr double kEarthRadius = 6378137.0;
  double length = 0.0;
  for (int i = 1; i < path.size(); ++i) {
    const double dLat = (path[i].lat - path[i - 1].lat) * M_PI / 180.0;
    const double dLon = (path[i].lon - path[i - 1].lon) * M_PI / 180.0;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(path[i - 1].lat * M_PI / 180.0) *
                         std::cos(path[i].lat * M_PI / 180.0) *
                         std::sin(dLon / 2) * std::sin(dLon / 2);
    length += kEarthRadius * (2 * std::atan2(std::sqrt(a), std::sqrt(1 - a)));
  }
  return length;
}

QVector<QVector3D> Path3DManager::PathPointsToWorld(
    const QVector<Path3DRecord>& path, bool keepHeight) const {
  QVector<QVector3D> points;
  points.reserve(path.size());
  if (!context_.latLonToWorldAt) return points;
  for (const Path3DRecord& record : path) {
    QVector3D world = context_.latLonToWorldAt(record.lat, record.lon,
                                               record.ref_lat, record.ref_lon);
    world.setY(keepHeight ? record.height : 0.0f);
    points.append(world);
  }
  return points;
}

QVector<QVector3D> Path3DManager::ResamplePathOnTerrain(
    const QVector<QVector3D>& controlPoints, float lift) const {
  if (controlPoints.size() < 2 || !context_.heightAtWorld) return controlPoints;
  QVector<QVector3D> output;
  const double maxDim =
      context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
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

Qt3DCore::QEntity* Path3DManager::BuildPathEntity(
    const QVector<QVector3D>& points) {
  if (points.isEmpty() || !context_.rootEntity) return nullptr;
  auto* entity = new Qt3DCore::QEntity();
  const QVector3D color(path_color_.redF(), path_color_.greenF(),
                        path_color_.blueF());
  const float halfWidth = PathHalfWidth();

  if (points.size() >= 2) {
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
      if (direction.lengthSquared() < 1e-12f)
        direction = QVector3D(1.0f, 0.0f, 0.0f);
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
    auto addAttribute = [geometry](Qt3DCore::QBuffer* buffer,
                                   const QString& name, uint vertexSize,
                                   uint attributeCount) {
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
    geometry->addAttribute(addAttribute(
        positionBuffer, Qt3DCore::QAttribute::defaultPositionAttributeName(), 3,
        uint(positions.size())));
    geometry->addAttribute(addAttribute(
        colorBuffer, Qt3DCore::QAttribute::defaultColorAttributeName(), 3,
        uint(colors.size())));

    auto* indexBuffer = new Qt3DCore::QBuffer(geometry);
    QByteArray indexData;
    indexData.resize(int(indices.size() * sizeof(unsigned int)));
    std::copy(indices.begin(), indices.end(),
              reinterpret_cast<unsigned int*>(indexData.data()));
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

    auto* material = new Qt3DExtras::QPerVertexColorMaterial(entity);
    if (auto* effect = material->effect())
      for (auto* technique : effect->techniques())
        for (auto* pass : technique->renderPasses()) {
          auto* cullFace = new Qt3DRender::QCullFace(pass);
          cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
          pass->addRenderState(cullFace);
        }
    entity->addComponent(renderer);
    entity->addComponent(material);
  }

  auto* sphere = new Qt3DExtras::QSphereMesh(entity);
  sphere->setRings(8);
  sphere->setSlices(12);
  const double maxDim =
      context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  sphere->setRadius(std::max(halfWidth * 0.9f, float(maxDim * 0.0015)));
  auto* material = new Qt3DExtras::QPhongMaterial(entity);
  material->setAmbient(path_color_);
  material->setDiffuse(QColor(20, 20, 20));
  material->setSpecular(QColor(255, 255, 255));
  material->setShininess(20.0f);
  for (const QVector3D& point : points) {
    auto* node = new Qt3DCore::QEntity(entity);
    auto* transform = new Qt3DCore::QTransform(node);
    transform->setTranslation(point);
    node->addComponent(sphere);
    node->addComponent(material);
    node->addComponent(transform);
  }
  entity->setParent(context_.rootEntity);
  if (context_.view) context_.view->requestUpdate();
  return entity;
}

void Path3DManager::AddPoint(const Path3DRecord& point) {
  current_path_.append(point);
  RebuildCurrentPathEntity();
  emit SignalPathPointAdded(current_path_.size());
}

void Path3DManager::SetAddingPath(bool enable) {
  if (!enable) return;
  destroyEntity(current_path_entity_);
  current_path_.clear();
}

void Path3DManager::RebuildCurrentPathEntity() {
  destroyEntity(current_path_entity_);
  if (current_path_.isEmpty()) return;
  current_path_entity_ = BuildPathEntity(ResamplePathOnTerrain(
      PathPointsToWorld(current_path_, true), PathLift()));
}

void Path3DManager::RefreshAllPathEntities() {
  RebuildCurrentPathEntity();
  for (int i = 0; i < saved_paths_.size(); ++i) {
    destroyEntity(saved_path_entities_[i]);
    saved_path_entities_[i] = BuildPathEntity(ResamplePathOnTerrain(
        PathPointsToWorld(saved_paths_[i], false), PathLift()));
  }
}

quint64 Path3DManager::SaveCurrentPath(const QString& name) {
  if (current_path_.isEmpty()) return 0;
  const quint64 id = next_path_id_++;
  const double length = ComputePathLengthMeters(current_path_);
  saved_paths_.append(current_path_);
  saved_path_ids_.append(id);
  saved_path_names_.append(name.isEmpty() ? QString("Path %1").arg(id) : name);
  saved_path_entities_.append(current_path_entity_);
  current_path_entity_ = nullptr;
  current_path_.clear();
  emit SignalPathSaved(id, saved_path_names_.last(), length);
  return id;
}

void Path3DManager::RemoveSavedPathById(quint64 id) {
  const int index = saved_path_ids_.indexOf(id);
  if (index < 0) return;
  destroyEntity(saved_path_entities_[index]);
  saved_path_ids_.removeAt(index);
  saved_path_names_.removeAt(index);
  saved_path_entities_.removeAt(index);
  saved_paths_.removeAt(index);
  emit SignalPathRemoved(id);
}

void Path3DManager::ClearAllPaths() {
  destroyEntity(current_path_entity_);
  current_path_.clear();
  for (auto*& entity : saved_path_entities_) destroyEntity(entity);
  saved_path_entities_.clear();
  saved_path_ids_.clear();
  saved_path_names_.clear();
  saved_paths_.clear();
  if (path_anim_timer_) {
    path_anim_timer_->stop();
    delete path_anim_timer_;
    path_anim_timer_ = nullptr;
  }
  destroyEntity(anim_path_entity_);
  anim_geom_renderer_ = nullptr;
  anim_pos_buffer_ = nullptr;
  anim_col_buffer_ = nullptr;
  anim_idx_buffer_ = nullptr;
  path_animating_ = false;
  anim_ctrl_pts_.clear();
}

void Path3DManager::SetPathStyle(int width, const QColor& color) {
  path_width_px_ = std::max(1, width);
  path_color_ = color;
  path_width_m_ = double(path_width_px_) * MetersPerWidthUnit();
  RefreshAllPathEntities();
  emit SignalPathStyleChanged();
}

void Path3DManager::SetAllPathsStyle(int width, const QColor& color) {
  SetPathStyle(width, color);
}

void Path3DManager::startPathGrowAnimation(
    const QVector<QVector3D>& controlPoints, bool isCurrent, int savedIndex) {
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
  anim_is_current_ = isCurrent;
  anim_path_index_ = savedIndex;

  if (isCurrent) {
    destroyEntity(current_path_entity_);
  } else if (savedIndex >= 0 && savedIndex < saved_path_entities_.size()) {
    destroyEntity(saved_path_entities_[savedIndex]);
  }

  path_anim_timer_ = new QTimer(this);
  connect(path_anim_timer_, &QTimer::timeout, this, [this]() {
    path_anim_progress_ += 1.0f / 60.0f;
    if (path_anim_progress_ >= 1.0f) {
      path_anim_progress_ = 1.0f;
      updatePathGrowFrame();
      path_anim_timer_->stop();
      delete path_anim_timer_;
      path_anim_timer_ = nullptr;
      onPathGrowFinished();
      return;
    }
    updatePathGrowFrame();
  });
  path_anim_timer_->start(50);
}

void Path3DManager::updatePathGrowFrame() {
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
  const QVector<QVector3D> groundPoints =
      ResamplePathOnTerrain(visible, PathLift());
  if (!anim_path_entity_) buildAnimEntityShell();
  updateAnimGeometry(groundPoints);
}

void Path3DManager::onPathGrowFinished() {
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
  if (anim_is_current_) {
    RebuildCurrentPathEntity();
  } else if (anim_path_index_ >= 0 && anim_path_index_ < saved_paths_.size()) {
    destroyEntity(saved_path_entities_[anim_path_index_]);
    saved_path_entities_[anim_path_index_] =
        BuildPathEntity(ResamplePathOnTerrain(
            PathPointsToWorld(saved_paths_[anim_path_index_], false),
            PathLift()));
  }
  emit SignalPathAnimationFinished();
}

void Path3DManager::PlayPathAnimation(int savedPathIndex) {
  if (savedPathIndex < 0 || savedPathIndex >= saved_paths_.size()) return;
  startPathGrowAnimation(PathPointsToWorld(saved_paths_[savedPathIndex], false),
                         false, savedPathIndex);
}

void Path3DManager::buildAnimEntityShell() {
  if (!context_.rootEntity) return;
  const int totalSegments = anim_ctrl_pts_.size() - 1;
  const double maxDim =
      context_.maxTerrainDimension ? context_.maxTerrainDimension() : 0.0;
  const float segmentLength = float(std::max(1.0, maxDim * 0.002));
  float maxSegmentWorldLength = 0.0f;
  for (int i = 0; i < totalSegments; ++i)
    maxSegmentWorldLength =
        std::max(maxSegmentWorldLength,
                 anim_ctrl_pts_[i].distanceToPoint(anim_ctrl_pts_[i + 1]));
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
  positionAttribute->setName(
      Qt3DCore::QAttribute::defaultPositionAttributeName());
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
  anim_geom_renderer_->setPrimitiveType(
      Qt3DRender::QGeometryRenderer::Triangles);
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

void Path3DManager::updateAnimGeometry(const QVector<QVector3D>& groundPoints) {
  if (!anim_path_entity_ || groundPoints.size() < 2) return;
  const int count = groundPoints.size();
  const QVector3D color(path_color_.redF(), path_color_.greenF(),
                        path_color_.blueF());
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
    if (direction.lengthSquared() < 1e-12f)
      direction = QVector3D(1.0f, 0.0f, 0.0f);
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
  std::copy(indices.begin(), indices.end(),
            reinterpret_cast<unsigned int*>(indexData.data()));
  anim_idx_buffer_->setData(indexData);
  for (auto* attribute : anim_geom_renderer_->geometry()->attributes())
    attribute->setCount(
        uint(attribute->attributeType() == Qt3DCore::QAttribute::IndexAttribute
                 ? indices.size()
                 : positions.size()));
  anim_geom_renderer_->setVertexCount(int(indices.size()));
  if (context_.view) context_.view->requestUpdate();
}

bool Path3DManager::ExportPaths(const QVector<quint64>& ids,
                                const QString& filePath, ExportFormat format) {
  if (ids.isEmpty() || filePath.isEmpty()) return false;

  QJsonArray pathsArray;

  for (quint64 id : ids) {
    int idx = saved_path_ids_.indexOf(id);
    if (idx < 0) continue;

    const auto& points = saved_paths_[idx];

    QJsonObject pathObj;
    pathObj["id"] = qint64(id);
    pathObj["name"] = saved_path_names_[idx];
    // pathObj["color"] = saved_path_colors_[idx].name(QColor::HexRgb);
    // pathObj["width"] = saved_path_widths_[idx];

    QJsonArray pointsArray;
    for (const auto& rec : points) {
      QJsonObject pt;
      pt["lat"] = rec.lat;
      pt["lon"] = rec.lon;
      pt["height"] = rec.height;
      pt["ref_lat"] = rec.ref_lat;
      pt["ref_lon"] = rec.ref_lon;
      pointsArray.append(pt);
    }
    pathObj["points"] = pointsArray;
    pathsArray.append(pathObj);
  }

  if (pathsArray.isEmpty()) return false;

  QJsonObject root;
  root["version"] = 1;
  root["paths"] = pathsArray;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "[export] cannot open file:" << filePath;
    return false;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  qDebug() << "[export] exported" << pathsArray.size() << "paths to"
           << filePath;
  return true;
}

int Path3DManager::ImportPaths(const QString& filePath,
                               QVector<quint64>& outIds) {
  outIds.clear();

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "[import] cannot open file:" << filePath;
    return 0;
  }

  QJsonParseError parseErr;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
  if (parseErr.error != QJsonParseError::NoError) {
    qWarning() << "[import] JSON parse error:" << parseErr.errorString();
    return 0;
  }

  const QJsonObject root = doc.object();
  const int version = root["version"].toInt(0);
  if (version != 1) {
    qWarning() << "[import] unsupported version:" << version;
    return 0;
  }

  const QJsonArray pathsArray = root["paths"].toArray();
  if (pathsArray.isEmpty()) return 0;

  int imported = 0;
  for (const auto& v : pathsArray) {
    const QJsonObject pathObj = v.toObject();

    // 解析点
    QVector<Path3DRecord> points;
    const QJsonArray pts = pathObj["points"].toArray();
    if (pts.size() < 2) {
      qWarning() << "[import] path skipped: less than 2 points";
      continue;
    }

    for (const auto& pv : pts) {
      const QJsonObject p = pv.toObject();
      points.append({p["lat"].toDouble(), p["lon"].toDouble(),
                     static_cast<float>(p["height"].toDouble()),
                     p["ref_lat"].toDouble(), p["ref_lon"].toDouble()});
    }

    // 分配新 ID（避免和已有路径冲突）
    const quint64 newId = next_path_id_++;

    // 名称去重：如果已存在同名，加后缀
    QString name = pathObj["name"].toString();
    if (saved_path_names_.contains(name)) {
      int suffix = 1;
      while (saved_path_names_.contains(
          QString("%1_imported_%2").arg(name).arg(suffix)))
        suffix++;
      name = QString("%1_imported_%2").arg(name).arg(suffix);
    }

    const QColor color = QColor(pathObj["color"].toString("#FF4737"));
    const int width = pathObj["width"].toInt(3);

    saved_paths_.append(points);
    saved_path_ids_.append(newId);
    saved_path_names_.append(name);
    // saved_path_colors_.append(color);
    // saved_path_widths_.append(width);

    // 重建 entity
    QVector<QVector3D> ctrl = PathPointsToWorld(points, false);
    QVector<QVector3D> groundPts = ResamplePathOnTerrain(ctrl, PathLift());
    saved_path_entities_.append(BuildPathEntity(groundPts));

    outIds.append(newId);
    emit SignalPathSaved(newId, name, ComputePathLengthMeters(points));
    imported++;
  }

  qDebug() << "[import] imported" << imported << "paths";
  return imported;
}