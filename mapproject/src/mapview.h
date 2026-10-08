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

enum ImageType {
  kImageTypeRoads,
  kImageTypeSt,
};
class MapView : public QGraphicsView {
  Q_OBJECT

 public:
  explicit MapView(QWidget* parent = nullptr);
  void SwitchTileServer(ImageType type);

 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private slots:
  void onTileLoaded(QNetworkReply* reply);
  void loadVisibleTiles();

 private:
  static QPoint latLonToTile(double lat, double lon, int zoom);
  static QNetworkRequest::Attribute epochAttr() {
    return static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);
  }
  void setSceneRectForZoom(int zoom);

  QGraphicsScene* m_scene;
  QNetworkAccessManager* m_nam;
  QTimer m_scrollTimer;

  int m_zoom;
  bool m_mouseDragging;
  QPoint m_lastMousePos;
  QSet<QPoint> m_loadedTiles;  // 已请求的瓦片坐标
  quint64 m_epoch = 0;

  QString url_ =
      "https://webrd0%1.is.autonavi.com/"
      "appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4";
  ;
};

#endif  // MAPVIEW_H