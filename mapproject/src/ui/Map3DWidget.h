#pragma once
#include <QWidget>

#include "Scene3DRenderer.h"

class TileDownloader;
// 3D 地图业务控件：负责把中心点/视口转换为瓦片请求，并驱动 DEM 到场景的构建。
class Map3DWidget : public QWidget {
  Q_OBJECT
 public:
  // 创建 3D 地图控件及其渲染、下载和数据处理对象。
  explicit Map3DWidget(QWidget* parent = nullptr);
  // 释放控件持有的处理对象；Qt 子对象由对象树管理。
  ~Map3DWidget() override;

  // 异步启动构建流程：下载影像和 DEM，拼接/解码后在 renderer_ 中生成地形。
  // centerLat/centerLon 为 WGS84 度数，radiusMeters 为覆盖半径（米）。
  void Build3DFromCenter(double centerLat, double centerLon,
                         double radiusMeters);

  // 修改显示模式，不重新下载数据。
  void UpdateMap3DShowType(Map3DShowType type);

  // 设置垂直夸张倍数并更新渲染器。
  void SetDemExaggeration(double k);  // 手动调垂直夸张

  // 根据视口尺寸和米/像素比例估算覆盖半径，再复用中心点构建流程。
  // metersPerPixel 是地面分辨率，viewportW/viewportH 是视口像素尺寸。
  void Build3DFromView(double centerLat, double centerLon,
                       double metersPerPixel, int viewportW, int viewportH);

  // 设置渲染器环境光强度，范围为 0 到 1。
  void SetAmbient(float v);
  // 设置光源方位角，单位为度。
  void SetLightAzimuth(float deg);
  // 设置光源高度角，单位为度。
  void SetLightElevation(float deg);
  // 设置渲染颜色亮度系数。
  void SetBrightness(float v);
  // 设置渲染颜色对比度系数。
  void SetContrast(float v);
  // 应用指定的光照预设。
  void ApplyLightPreset(int idx);

  // 只影响"正在画 + 之后新建"的路径
  // 设置未来新建路径的像素线宽和颜色。
  void SetPathStyle(int width, const QColor& color);
  // 额外把已保存的所有路径一起改样式
  // 将像素线宽和颜色应用到所有路径。
  void SetAllPathsStyle(int width, const QColor& color);
  void PlayPathAnimation(int savedPathIndex);
  int GetPathIndexById(quint64 id) const;
  bool ExportPaths(const QVector<quint64>& ids, const QString& filePath,
                   ExportFormat format = ExportFormat::JSON);
  int ImportPaths(const QString& filePath, QVector<quint64>& outIds);

 signals:
  // 影像和 DEM 数据处理完成，3D 地形已经可以显示。
  void SignalReady();
  // 构建流程失败时发出可读错误原因。
  void SignalFailed(const QString& reason);
  // 3D 路径保存完成，lengthMeters 是路径长度（米）。
  void SignalPathSaved(quint64 id, const QString& name, double lengthMeters);
  // 3D 已保存路径删除通知。
  void SignalPathRemoved(quint64 id);
  // 3D 当前路径新增点后发出点数。
  void SignalPathPointAdded(int count);
  // 地形网格或地理原点发生变化。
  void TerrainChanged();
  // 路径样式发生变化。
  void PathStyleChanged();
  void SignalPathAnimationFinished();

 public slots:
  // 开启或关闭 3D 视图中的路径采集。
  void SlotSetAddingPath(bool enable);
  // 保存当前 3D 路径并返回路径 ID。
  quint64 SlotSaveCurrentPath(const QString& name = QString());
  // 删除指定 ID 的已保存 3D 路径。
  void SlotRemoveSavedPathById(quint64 id);
  // 清除当前 3D 路径及全部已保存路径。
  void SlotClearAllPaths();

 private:
  // 负责显示地形及接收用户交互的渲染器。
  Scene3DRenderer* renderer_;
  // 负责异步获取影像和 DEM 瓦片的网络下载器。
  TileDownloader* downloader_;
  Map3DShowType type_ = kMap3DShowTypeMesh;
  QSharedPointer<QMap<QPair<int, int>, QByteArray>> last_img_shared_;
  QSharedPointer<QMap<QPair<int, int>, QByteArray>> last_cia_shared_;
  int last_dx0_, last_dy0_, last_dx1_, last_dy1_;
};