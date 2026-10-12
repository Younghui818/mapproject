#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPoint>
#include <QSet>
#include <QTimer>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QVector>

enum MapSources {
  kMapSourcesAMap = 0,
  kMapSourcesTianDiTu
};

enum ImageType {
  kImageTypeRoads,
  kImageTypeSt,
};

enum TileLayerId { LayerSat = 0, LayerAnno = 1 };

struct TileLayer {
  TileLayerId id;
  QString url_template;
  double opacity = 1.0;
  int zBase = 0;
  bool enabled = false;
};

class MapView : public QGraphicsView {
  Q_OBJECT

 public:
  explicit MapView(QWidget* parent = nullptr);
  void SwitchTileServer(MapSources sources, ImageType type, bool show_cva);

  // 路径绘制接口
 public slots:
  void SetAddingPath(bool enable);
  void ClearAllPaths();
  // 主动结束并保存当前路径（返回路径 id 或 0 表示失败/无路径）
  quint64 SaveCurrentPath(const QString& name);
  void RemoveSavedPathById(quint64 id);

 signals:
  void PathSaved(quint64 id, const QString& name);
  void PathRemoved(quint64 id);

 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private slots:
  void SlotOnTileLoaded(QNetworkReply* reply);
  void SlotLoadVisibleTiles();

 private:
  static QPoint LatLonToTile(double lat, double lon, int zoom);
  static QNetworkRequest::Attribute EpochAttr() {
    return static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);
  }
  void SetSceneRectForZoom(int zoom);

  // 路径绘制内部
  void AddPointToCurrentPath(const QPointF& pt);

  // 仅清除瓦片，不影响路径/标记
  void ClearTileItems();
  // 缩放时按比例缩放路径（标记使用忽略变换保持像素大小）
  void ScalePaths(double factor);

  QGraphicsScene* scene_;
  QNetworkAccessManager* nam_;
  QTimer scroll_timer_;

  int zoom_;
  bool mouse_dragging_;
  QPoint last_mouse_pos_;
  quint64 epoch_ = 0;

  // click/drag detection when in adding mode
  bool potential_click_ = false;
  QPoint press_pos_;
  const int click_threshold_ = 6;  // 像素阈值，超过认为是拖拽

  QVector<TileLayer> layers_;
  QHash<int, QSet<QPoint>> loaded_tiles_;
  QNetworkRequest::Attribute layer_attr_ =
      QNetworkRequest::Attribute(QNetworkRequest::UserMax + 1);

  // 路径绘制相关成员
  bool adding_path_ = false;
  QVector<QPointF> current_path_points_;
  QVector<QGraphicsEllipseItem*> current_path_markers_;
  QGraphicsPathItem* current_path_item_ = nullptr;

  // 已保存路径（按 id 管理）
  QVector<QGraphicsPathItem*> saved_path_items_;
  QVector<QVector<QGraphicsEllipseItem*>> saved_path_markers_;
  QVector<quint64> saved_path_ids_;
  QVector<QString> saved_path_names_;

  QPen path_pen_;
  QBrush marker_brush_;
  double marker_pixel_radius_ = 4.0;  // 在屏幕像素上的半径

  // id 生成
  quint64 next_path_id_ = 1;
};

#endif  // MAPVIEW_H