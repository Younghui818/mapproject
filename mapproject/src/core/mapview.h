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

enum MapSources {
  kMapSourcesAMap = 0,  // 高德地图
  kMapSourcesTiamDiTu   // 天地图

};

enum ImageType {
  kImageTypeRoads,  // 标准地图
  kImageTypeSt,     // 卫星地图
};

enum TileLayerId { LayerSat = 0, LayerAnno = 1 };

struct TileLayer {
  TileLayerId id;
  QString url_template;  // %1 sub %2 x %3 y %4 z
  double opacity = 1.0;
  int zBase = 0;  // 卫星0，注记1
  bool enabled = false;
};

class MapView : public QGraphicsView {
  Q_OBJECT

 public:
  explicit MapView(QWidget* parent = nullptr);
  void SwitchTileServer(MapSources sources, ImageType type, bool show_cva);

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

  QGraphicsScene* scene_;
  QNetworkAccessManager* nam_;
  QTimer scroll_timer_;

  int zoom_;
  bool mouse_dragging_;
  QPoint last_mouse_pos_;
  quint64 epoch_ = 0;

  QVector<TileLayer> layers_;
  // 加载记录按图层分开，避免卫星和注记 key 冲突
  QHash<int, QSet<QPoint>> loaded_tiles_;  // layerId -> keys
  QNetworkRequest::Attribute layer_attr_ =
      QNetworkRequest::Attribute(QNetworkRequest::UserMax + 1);
};

#endif  // MAPVIEW_H