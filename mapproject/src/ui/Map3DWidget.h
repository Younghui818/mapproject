#pragma once
#include <QWidget>

#include "Scene3DRenderer.h"

class TileDownloader;
class TileStitcher;
class DemProcessor;
class MeshBuilder;

class Map3DWidget : public QWidget {
  Q_OBJECT
 public:
  explicit Map3DWidget(QWidget* parent = nullptr);
  ~Map3DWidget() override;

  // 异步启动构建流程
  void Build3DFromCenter(double centerLat, double centerLon,
                         double radiusMeters);

  void UpdateMap3dShowType(Map3DShowType type);

  void Build3DFromDemTile(double centerLat, double centerLon, int zoom = 12);

  void SetDemExaggeration(double k);  // 手动调垂直夸张

  // 新增：直接吃 zoom + 视口，内部自己算半径
  void Build3DFromView(double centerLat, double centerLon,
                       double metersPerPixel, int viewportW, int viewportH);

  void SetAmbient(float v);
  void SetLightAzimuth(float deg);
  void SetLightElevation(float deg);
  void SetBrightness(float v);
  void SetContrast(float v);
  void ApplyLightPreset(int idx);

  // 只影响"正在画 + 之后新建"的路径
  void SetPathStyle(int width, const QColor& color);
  // 额外把已保存的所有路径一起改样式
  void SetAllPathsStyle(int width, const QColor& color);

 signals:
  void SignalReady();
  void SignalFailed(const QString& reason);
  void SignalPathSaved(quint64 id, const QString& name, double lengthMeters);
  void SignalPathRemoved(quint64 id);
  void SignalPathPointAdded(int count);

 public slots:
  void SlotSetAddingPath(bool enable);
  quint64 SlotSaveCurrentPath(const QString& name = QString());
  void SlotRemoveSavedPathById(quint64 id);
  void SlotClearAllPaths();

 private:
  Scene3DRenderer* renderer_;
  TileDownloader* downloader_;
  TileStitcher* stitcher_;
  DemProcessor* dem_;
  MeshBuilder* meshBuilder_;

  Map3DShowType type_ = kMap3DShowTypePoint;
};