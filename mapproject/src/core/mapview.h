#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPen>
#include <QPoint>
#include <QSet>
#include <QTimer>
#include <QVector>

// 在线地图数据源。
enum MapSources {
  kMapSourcesAMap = 0,  // 高德地图服务
  kMapSourcesTianDiTu   // 天地图服务
};

// 地图底图类型。
enum ImageType {
  kImageTypeRoads,  // 标准道路底图
  kImageTypeSt      // 卫星影像底图
};

// 一个 MapView 可以同时加载影像和注记两个图层。
enum TileLayerId {
  LayerSat = 0,  // 卫星影像层
  LayerAnno = 1  // 透明注记层
};

// 描述一个可独立开关、设置透明度和绘制层级的瓦片图层。
struct TileLayer {
  TileLayerId id;        // 图层唯一类型标识
  QString url_template;  // 包含瓦片坐标占位符的 URL 模板
  double opacity = 1.0;  // 绘制透明度，范围通常为 0.0 到 1.0
  int zBase = 0;         // 场景绘制层级，数值越大越靠前
  bool enabled = false;  // 是否参与当前可见区域的瓦片请求
};

// 基于 QGraphicsView 的 2D 地图控件，负责瓦片显示、缩放平移和路径交互。
class MapView : public QGraphicsView {
  Q_OBJECT

 public:
  // 创建地图场景、网络请求管理器并加载初始视口瓦片。
  explicit MapView(QWidget* parent = nullptr);
  // 根据数据源、底图类型和注记开关重新配置图层并加载当前视口瓦片。
  void SwitchTileServer(MapSources sources, ImageType type, bool show_cva);
  // 将 scene 坐标转换为 WGS84 经纬度。
  // 输出参数 outLat/outLon 分别接收纬度和经度；坐标无效时返回 false。
  bool SceneToLatLon(const QPointF& scenePt, double& outLat,
                     double& outLon) const;
  // 只影响"正在画 + 之后新建"的路径
  // width 为绘制线宽（像素），color 为路径颜色。
  void SetPathStyle(int width, const QColor& color);
  // 额外把已保存的所有路径一起改样式
  // 将同一线宽和颜色应用到当前路径及所有已保存路径。
  void SetAllPathsStyle(int width, const QColor& color);

 public slots:
  // 开启或关闭地图上的路径点采集模式。
  void SlotSetAddingPath(bool enable);
  // 删除当前未保存路径和全部已保存路径。
  void SlotClearAllPaths();
  // 主动结束并保存当前路径（返回路径 id 或 0 表示失败/无路径）
  quint64 SlotSaveCurrentPath(const QString& name);
  // 按唯一 ID 删除已保存路径；不存在时不产生更改。
  void SlotRemoveSavedPathById(quint64 id);

 signals:
  // 路径保存完成通知；lengthMeters 为按地图投影估算的路径长度（米）。
  void SignalPathSaved(quint64 id, const QString& name, double lengthMeters);
  // 已保存路径被删除时发出其唯一 ID。
  void SignalPathRemoved(quint64 id);

 protected:
  // 处理滚轮缩放，并同步缩放场景中的路径几何。
  void wheelEvent(QWheelEvent* event) override;
  // 记录点击/拖拽起始位置，并按当前模式准备加点或平移。
  void mousePressEvent(QMouseEvent* event) override;
  // 根据鼠标移动更新拖拽位置或点击/拖拽判定。
  void mouseMoveEvent(QMouseEvent* event) override;
  // 完成路径加点或结束本次拖拽操作。
  void mouseReleaseEvent(QMouseEvent* event) override;

 private slots:
  // 解码网络响应，并将有效瓦片放置到对应场景坐标。
  void SlotOnTileLoaded(QNetworkReply* reply);
  // 计算当前视口缺少的瓦片并发起下载。
  void SlotLoadVisibleTiles();

 private:
  // 将 WGS84 经纬度转换为指定缩放级别的 Web Mercator 瓦片坐标。
  static QPoint LatLonToTile(double lat, double lon, int zoom);

  // 请求属性键：保存发起请求时的视口代数，用于过滤过期响应。
  static QNetworkRequest::Attribute EpochAttr() {
    return static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);
  }
  // 根据缩放级别设置 Web Mercator 世界范围对应的场景矩形。
  void SetSceneRectForZoom(int zoom);

  // 路径绘制内部
  // 把场景坐标点追加到当前路径，并更新线条和点标记。
  void AddPointToCurrentPath(const QPointF& pt);

  // 仅清除瓦片，不影响路径/标记
  // 删除场景中标记为瓦片的图元。
  void ClearTileItems();
  // 缩放时按比例缩放路径（标记使用忽略变换保持像素大小）
  // factor 为旧缩放级别到新缩放级别的场景尺度比例。
  void ScalePaths(double factor);
  void RefreshPathPen();  // 依据 pen_width_ / pen_color_ 重建 path_pen_
  // 根据相邻场景坐标转换得到的经纬度计算整条路径长度（米）。
  double ComputePathLengthMeters(const QVector<QPointF>& pts) const;

  QGraphicsScene* scene_;       // 地图瓦片和路径图元所在场景，由本控件持有
  QNetworkAccessManager* nam_;  // 异步请求瓦片的网络管理器，由本控件持有
  QTimer scroll_timer_;         // 滚动/缩放后的延迟加载定时器

  int zoom_;               // 当前 Web Mercator 缩放级别
  bool mouse_dragging_;    // 是否正在通过鼠标拖动平移地图
  QPoint last_mouse_pos_;  // 上一次鼠标位置，用于计算拖动增量
  quint64 epoch_ = 0;      // 视口代数；改变视图或图层时递增以过滤旧响应

  // 添加路径模式下用于区分“点击加点”和“拖拽地图”的状态。
  bool potential_click_ = false;   // 当前按压仍可能被判定为点击
  QPoint press_pos_;               // 鼠标按下位置（控件像素坐标）
  const int click_threshold_ = 6;  // 超过该像素距离后判定为拖拽

  // 图层配置和已加载瓦片集合；集合用于避免重复下载同一瓦片。
  QVector<TileLayer> layers_;  // 当前启用的瓦片图层配置
  QHash<int, QSet<QPoint>>
      loaded_tiles_;  // 按图层 ID 保存已请求/加载的瓦片坐标
  // 网络请求属性键：记录每个请求所属的 TileLayerId。
  QNetworkRequest::Attribute layer_attr_ =
      QNetworkRequest::Attribute(QNetworkRequest::UserMax + 1);

  // 路径绘制相关成员
  bool adding_path_ = false;  // 是否处于路径采集模式
  QVector<QPointF>
      current_path_points_;  // 当前路径的场景坐标点，按绘制顺序排列
  QVector<QGraphicsEllipseItem*>
      current_path_markers_;  // 当前路径各点对应的圆形标记
  QGraphicsPathItem* current_path_item_ = nullptr;  // 当前路径的折线图元

  // 已保存路径（按 id 管理）
  QVector<QGraphicsPathItem*> saved_path_items_;  // 每条已保存路径的折线图元
  QVector<QVector<QGraphicsEllipseItem*>>
      saved_path_markers_;           // 各路径的顶点标记
  QVector<quint64> saved_path_ids_;  // 与其他已保存路径数组按索引对应的唯一 ID
  QVector<QString> saved_path_names_;     // 各路径的显示名称
  QVector<double> saved_path_lengths_m_;  // 各路径的长度，单位为米

  QPen path_pen_;                     // 当前路径线条画笔
  QBrush marker_brush_;               // 路径点标记的填充画刷
  double marker_pixel_radius_ = 4.0;  // 路径标记在屏幕上的半径（像素）

  // id 生成
  quint64 next_path_id_ = 1;  // 下一个分配给新路径的唯一 ID

  // ==== 路径线宽 / 颜色（供 SetPathStyle / SetAllPathsStyle 使用）====
  int pen_width_ = 2;           // 当前路径线宽（像素）
  QColor pen_color_ = Qt::red;  // 当前路径颜色
};

#endif  // MAPVIEW_H