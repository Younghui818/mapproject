#pragma once
#include <QWidget>

class Scene3DRenderer;
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
  void Build3DFromCenter(double centerLat, double centerLon, double radiusMeters);

signals:
  void SignalReady();
  void Failed(const QString& reason);

private:
  Scene3DRenderer* renderer_;
  TileDownloader* downloader_;
  TileStitcher* stitcher_;
  DemProcessor* dem_;
  MeshBuilder* meshBuilder_;
};