#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <functional>

#include "PathDataModel.h"
#include "RouteService.h"

namespace Qt3DCore {
class QBuffer;
class QEntity;
}  // namespace Qt3DCore

namespace Qt3DExtras {
class Qt3DWindow;
}

namespace Qt3DRender {
class QGeometryRenderer;
}

class QTimer;

class Path3DManager : public QObject {
  Q_OBJECT

 public:
  struct Context {
    Qt3DCore::QEntity* rootEntity = nullptr;
    Qt3DExtras::Qt3DWindow* view = nullptr;
    std::function<QVector3D(double, double, double, double)> latLonToWorldAt;
    std::function<float(const QVector3D&)> heightAtWorld;
    std::function<double()> maxTerrainDimension;
    std::function<double()> metersPerWidthUnit;
  };

  explicit Path3DManager(Context context, QObject* parent = nullptr);

  void AddPoint(const PathRecord& point);
  void SetAddingPath(bool enable);
  PathData GetCurrentPathPoints();

  quint64 SaveCurrentPath(const QString& name = QString());
  void RemoveSavedPathById(quint64 id);
  void ClearAllPaths();
  void SetPathStyle(int width, const QColor& color);
  void SetAllPathsStyle(int width, const QColor& color);
  void PlayPathAnimation(PathData path_data);
  void RefreshAllPathEntities();
  // 导出指定路径
  bool ExportPaths(const QVector<quint64>& ids, const QString& filePath, ExportFormat format = ExportFormat::JSON);
  int ImportPaths(const QString& filePath, QVector<quint64>& outIds);
  void BuildPathEntities(PathData path_data);

 signals:
  void SignalPathPointAdded();
  void SignalPathSaved(PathData path_data);
  void SignalPathRemoved(quint64 id);
  void SignalPathStyleChanged();
  void SignalPathAnimationFinished();

 private:
  void DestroyEntity(Qt3DCore::QEntity*& entity);
  float PathHalfWidth() const;
  float PathLift() const;
  double MetersPerWidthUnit() const;

  QVector<QVector3D> ResamplePathOnTerrain(const QVector<QVector3D>& controlPoints, float lift) const;
  QVector<QVector3D> PathPointsToWorld(const QVector<PathRecord>& path, bool keepHeight) const;
  Qt3DCore::QEntity* BuildPathEntity(const QVector<QVector3D>& points);
  void RebuildCurrentPathEntity();

  void StartPathGrowAnimation(const QVector<QVector3D>& controlPoints, int savedIndex = -1);
  void UpdatePathGrowFrame();
  void OnPathGrowFinished();
  void BuildAnimEntityShell();
  void UpdateAnimGeometry(const QVector<QVector3D>& groundPoints);

  Context context_;

  // 当前路径
  PathData current_path_;
  Qt3DCore::QEntity* current_path_entity_ = nullptr;

  // 已保存路径渲染（按 id 管理）
  QMap<quint64, Qt3DCore::QEntity*> saved_path_entities_map_;

  quint64 next_path_id_ = 1;

  QColor current_path_color_{255, 180, 0};
  double current_path_width_m_ = 0.0;
  int current_path_width_px_ = 3;

  // 动画部分
  QVector<QVector3D> anim_ctrl_pts_;
  Qt3DCore::QEntity* anim_path_entity_ = nullptr;
  QTimer* path_anim_timer_ = nullptr;
  float path_anim_progress_ = 0.0f;
  bool path_animating_ = false;
  bool anim_is_current_ = true;
  int anim_path_id_ = -1;
  QVector<PathRecord> anim_path_point_;
  Qt3DRender::QGeometryRenderer* anim_geom_renderer_ = nullptr;
  Qt3DCore::QBuffer* anim_pos_buffer_ = nullptr;
  Qt3DCore::QBuffer* anim_col_buffer_ = nullptr;
  Qt3DCore::QBuffer* anim_idx_buffer_ = nullptr;
  int anim_max_verts_ = 0;
  int anim_max_idx_ = 0;
};
