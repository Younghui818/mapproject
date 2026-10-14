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

 signals:
  void SignalReady();
  void Failed(const QString& reason);

 private:
  Scene3DRenderer* renderer_;
  TileDownloader* downloader_;
  TileStitcher* stitcher_;
  DemProcessor* dem_;
  MeshBuilder* meshBuilder_;

  Map3DShowType type_ = kMap3DShowTypePoint;

  // DEMO
  void Show3DImageDemo();
};