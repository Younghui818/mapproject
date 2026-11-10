#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <functional>

#include "Path3DRecord.h"

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

  void AddPoint(const Path3DRecord& point);
  void SetAddingPath(bool enable);
  quint64 SaveCurrentPath(const QString& name = QString());
  void RemoveSavedPathById(quint64 id);
  void ClearAllPaths();
  void SetPathStyle(int width, const QColor& color);
  void SetAllPathsStyle(int width, const QColor& color);
  void PlayPathAnimation(int savedPathIndex);
  void RefreshAllPathEntities();

  const QVector<quint64>& GetSavedPathIds() const { return saved_path_ids_; }
  // 导出指定路径
  bool ExportPaths(const QVector<quint64>& ids, const QString& filePath,
                   ExportFormat format = ExportFormat::JSON);
  int ImportPaths(const QString& filePath, QVector<quint64>& outIds);

 signals:
  void SignalPathPointAdded(int count);
  void SignalPathSaved(quint64 id, const QString& name, double lengthMeters);
  void SignalPathRemoved(quint64 id);
  void SignalPathStyleChanged();
  void SignalPathAnimationFinished();

 private:
  void destroyEntity(Qt3DCore::QEntity*& entity);
  float PathHalfWidth() const;
  float PathLift() const;
  double MetersPerWidthUnit() const;
  double ComputePathLengthMeters(const QVector<Path3DRecord>& path) const;
  QVector<QVector3D> ResamplePathOnTerrain(
      const QVector<QVector3D>& controlPoints, float lift) const;
  QVector<QVector3D> PathPointsToWorld(const QVector<Path3DRecord>& path,
                                       bool keepHeight) const;
  Qt3DCore::QEntity* BuildPathEntity(const QVector<QVector3D>& points);
  void RebuildCurrentPathEntity();
  void startPathGrowAnimation(const QVector<QVector3D>& controlPoints,
                              bool isCurrent = true, int savedIndex = -1);
  void updatePathGrowFrame();
  void onPathGrowFinished();
  void buildAnimEntityShell();
  void updateAnimGeometry(const QVector<QVector3D>& groundPoints);

  Context context_;

  QVector<Path3DRecord> current_path_;
  Qt3DCore::QEntity* current_path_entity_ = nullptr;
  QVector<QVector<Path3DRecord>> saved_paths_;
  QVector<quint64> saved_path_ids_;
  QVector<QString> saved_path_names_;
  QVector<Qt3DCore::QEntity*> saved_path_entities_;
  quint64 next_path_id_ = 1;
  QColor path_color_{230, 60, 60};
  double path_width_m_ = 0.0;
  int path_width_px_ = 3;

  QVector<QVector3D> anim_ctrl_pts_;
  Qt3DCore::QEntity* anim_path_entity_ = nullptr;
  QTimer* path_anim_timer_ = nullptr;
  float path_anim_progress_ = 0.0f;
  bool path_animating_ = false;
  bool anim_is_current_ = true;
  int anim_path_index_ = -1;
  Qt3DRender::QGeometryRenderer* anim_geom_renderer_ = nullptr;
  Qt3DCore::QBuffer* anim_pos_buffer_ = nullptr;
  Qt3DCore::QBuffer* anim_col_buffer_ = nullptr;
  Qt3DCore::QBuffer* anim_idx_buffer_ = nullptr;
  int anim_max_verts_ = 0;
  int anim_max_idx_ = 0;
};
