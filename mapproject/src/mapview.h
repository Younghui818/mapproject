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

enum TileLayerId { LayerSat = 0, LayerAnno = 1 };

struct TileLayer {
  TileLayerId id;
  QString urlTemplate;  // %1 sub %2 x %3 y %4 z
  double opacity = 1.0;
  int zBase = 0;  // 卫星0，注记1
  bool enabled = false;
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
  quint64 m_epoch = 0;

  QVector<TileLayer> m_layers;
  // 加载记录按图层分开，避免卫星和注记 key 冲突
  QHash<int, QSet<QPoint>> m_loadedTiles;  // layerId -> keys
  QNetworkRequest::Attribute LayerAttr =
      QNetworkRequest::Attribute(QNetworkRequest::UserMax + 1);
};

#endif  // MAPVIEW_H