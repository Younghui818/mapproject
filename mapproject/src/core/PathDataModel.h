#pragma once

#include <QVector>
#include <QColor>
#include <QObject>
#include <QString>
#include <QMap>

enum class ExportFormat { JSON };

/**
 * 路径点角色
 */
enum class RoutePointRole { Start, Via, End };

// 路径点记录（与渲染无关，纯地理数据）
struct PathRecord {
  double lat = 0.0;
  double lon = 0.0;
  float height = 0.0f;
  double ref_lat = 0.0;
  double ref_lon = 0.0;
  RoutePointRole role = RoutePointRole::Via;
};

// 单条路径
struct PathData {
  quint64 id = 0;
  QString name;
  QVector<PathRecord> points;
  QVector<PathRecord> polyline;  // 规划后的折线（经纬度）
  QColor color = QColor(255, 71, 55);
  int width = 3;  // px (2D) 或 映射后的 3D 宽度
};

class PathDataModel : public QObject {
  Q_OBJECT

 public:
  explicit PathDataModel(QObject* parent = nullptr);

  quint64 AddPath(PathData data);
  void RemovePath(quint64 id);
  void ClearAll();

  // ---- 查询 ----
  const QMap<quint64, PathData>& GetAllPaths() const { return paths_map_; }
  PathData GetPath(quint64 id) const;
  int PathCount() const { return paths_map_.size(); }

  // ---- 修改 ----
  void SetPathStyle(quint64 id, const QColor& color, int width);
  void RenamePath(quint64 id, const QString& name);

  // ---- 计算 ----
  static double ComputeLengthMeters(const QVector<PathRecord>& points);

  // ---- 序列化 ----
  bool ExportPathToFile(const QString& filePath, const QVector<quint64>& ids) const;
  int ImportPathFromFile(const QString& filePath, QVector<quint64>& outIds);

  quint64 GetPathNewID();

 signals:
  void SignalPathAdded(quint64 id, const QString& name, double length);
  void SignalPathRemoved(quint64 id);
  void SignalPathStyleChanged(quint64 id);
  void SignalModelChanged();  // 通用通知

 private:
  QMap<quint64, PathData> paths_map_;
  quint64 next_id_ = 1;
};
