#include "mapview.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QImageReader>
#include <QNetworkProxy>
#include <QScrollBar>
#include <cmath>

MapView::MapView(QWidget* parent)
    : QGraphicsView(parent),
      m_scene(new QGraphicsScene(this)),
      m_nam(new QNetworkAccessManager(this)),
      m_zoom(12),
      m_mouseDragging(false) {
  setRenderHint(QPainter::SmoothPixmapTransform);
  setBackgroundBrush(QBrush(QColor(191, 191, 191)));
  setScene(m_scene);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QNetworkProxyFactory::setUseSystemConfiguration(true);

  connect(m_nam, &QNetworkAccessManager::finished, this,
          &MapView::onTileLoaded);

  m_scrollTimer.setSingleShot(true);
  connect(&m_scrollTimer, &QTimer::timeout, this, &MapView::loadVisibleTiles);
  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
          [this] { m_scrollTimer.start(150); });
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this] { m_scrollTimer.start(150); });

  int z = m_zoom;
  QPoint hz = latLonToTile(30.2741, 120.1551, z);
  QPointF centerScene((hz.x() + 0.5) * 256.0, (hz.y() + 0.5) * 256.0);

  setSceneRectForZoom(z);  // 整世界场景矩形
  centerOn(centerScene);
  loadVisibleTiles();
  qDebug() << "[Init map view center] center:x" << centerScene.x()
           << " y:" << centerScene.y();
}

void MapView::wheelEvent(QWheelEvent* event) {
  // 记住当前场景中心（zoom 改变后需要换算）
  QPointF oldCenter = mapToScene(viewport()->rect().center());
  qDebug() << "[Scale map view old center] center:x" << oldCenter.x()
           << " y:" << oldCenter.y();
  int oldZoom = m_zoom;

  if (event->angleDelta().y() > 0 && m_zoom < 18)
    m_zoom++;
  else if (event->angleDelta().y() < 0 && m_zoom > 3)
    m_zoom--;
  else {
    return;
  }

  for (auto& s : m_loadedTiles) s.clear();
  m_scene->clear();
  ++m_epoch;
  // 场景 rect 随 zoom 改变
  setSceneRectForZoom(m_zoom);

  // 场景坐标按 zoom 差值等比缩放，保持地理位置不变
  double factor = std::pow(2.0, m_zoom - oldZoom);
  QPointF newCenter(oldCenter.x() * factor, oldCenter.y() * factor);
  centerOn(newCenter);
  qDebug() << "[Scale map view new center] center:x" << newCenter.x()
           << " y:" << newCenter.y();
  loadVisibleTiles();
  m_scrollTimer.stop();
}

void MapView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_mouseDragging = true;
    m_lastMousePos = event->pos();
    setCursor(Qt::OpenHandCursor);
  }
  QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event) {
  if (m_mouseDragging) {
    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();
    // ★ 无 transform，视口像素 = 场景单位，直接累加
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
  }
  QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_mouseDragging = false;
    setCursor(Qt::ArrowCursor);
  }
  QGraphicsView::mouseReleaseEvent(event);
}

void MapView::onTileLoaded(QNetworkReply* reply) {
  auto key = reply->request().attribute(QNetworkRequest::User).toPoint();
  int layerId = reply->request().attribute(LayerAttr).toInt();
  quint64 ep = reply->request().attribute(epochAttr()).toULongLong();
  if (ep != m_epoch) {
    reply->deleteLater();
    return;
  }

  int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QByteArray ctype =
      reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
  QByteArray data = reply->readAll();

  if (reply->error() != QNetworkReply::NoError) {
    m_loadedTiles[layerId].remove(key);
    reply->deleteLater();
    return;
  }

  QImageReader r;
  r.setDecideFormatFromContent(true);
  QBuffer buf(&data);
  r.setDevice(&buf);
  if (!r.canRead()) {
    qDebug() << "[tile] reader cannot read, err=" << r.errorString()
             << "formats=" << QImageReader::supportedImageFormats();
    m_loadedTiles[layerId].remove(key);
    reply->deleteLater();
    return;
  }
  QImage img;
  if (r.read(&img) && !img.isNull()) {
    auto* item = new QGraphicsPixmapItem(QPixmap::fromImage(img));
    item->setPos(key.x() * 256.0, key.y() * 256.0);
    // 找该layer的opacity/z
    double op = 1.0;
    int z = 0;
    for (auto& l : m_layers)
      if (l.id == TileLayerId(layerId)) {
        op = l.opacity;
        z = l.zBase;
      }
    item->setOpacity(op);
    item->setZValue(z);
    m_scene->addItem(item);
  } else {
    m_loadedTiles[layerId].remove(key);
  }
  reply->deleteLater();
}

void MapView::loadVisibleTiles() {
  QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();

  qDebug() << "[Get map view rect] x:" << visibleRect.x()
           << " y:" << visibleRect.y() << " width:" << visibleRect.width()
           << " height:" << visibleRect.height();

  const double tileSize = 256.0;  // 瓦片永远 256 场景单位

  int left = static_cast<int>(std::floor(visibleRect.left() / tileSize));
  int right = static_cast<int>(std::floor(visibleRect.right() / tileSize));
  int top = static_cast<int>(std::floor(visibleRect.top() / tileSize));
  int bottom = static_cast<int>(std::floor(visibleRect.bottom() / tileSize));
  int maxT = (1 << m_zoom) - 1;

  for (const auto& ly : m_layers) {
    if (!ly.enabled) continue;
    for (int x = left; x <= right; ++x) {
      for (int y = top; y <= bottom; ++y) {
        if (x < 0 || x > maxT || y < 0 || y > maxT) continue;
        QPoint key(x, y);
        if (m_loadedTiles[ly.id].contains(key)) continue;
        int sub = (x + y) % 4 + 1;
        QString url;
        if (ly.urlTemplate.contains("tianditu")) {
          // 天地图WMTS示例：col=x,row=y,matrix=zoom
          url = ly.urlTemplate.arg(sub).arg(x).arg(y).arg(
              m_zoom);  // 按你模板占位调整
        } else {
          url = ly.urlTemplate.arg(sub).arg(x).arg(y).arg(m_zoom);
        }

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 MapStudio/1.0");
        req.setRawHeader("Referer", "https://www.amap.com/");
        req.setAttribute(QNetworkRequest::User, QVariant::fromValue(key));
        req.setAttribute(epochAttr(), QVariant::fromValue(m_epoch));
        req.setAttribute(LayerAttr,
                         int(ly.id));  // 自定义attr存layer
        m_nam->get(req);
        m_loadedTiles[ly.id].insert(key);
      }
    }
  }
}

QPoint MapView::latLonToTile(double lat, double lon, int zoom) {
  constexpr double PI = 3.14159265358979323846;
  double latRad = lat * PI / 180.0;
  int n = 1 << zoom;
  int x = static_cast<int>((lon + 180.0) / 360.0 * n);
  int y = static_cast<int>(
      (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / PI) / 2.0 *
      n);
  return QPoint(x, y);
}

void MapView::setSceneRectForZoom(int zoom) {
  double world = 256.0 * (1 << zoom);
  m_scene->setSceneRect(0, 0, world, world);
}

void MapView::SwitchTileServer(ImageType type) {
  m_layers.clear();
  if (type == kImageTypeSt) {
    // 卫星层
    m_layers.append({LayerSat,
                     "https://t%1.tianditu.gov.cn/img_w/wmts?"
                     "SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=img&"
                     "STYLE=default&FORMAT=tiles&TILEMATRIXSET=w&"
                     "TILEMATRIX=%4&TILEROW=%3&TILECOL=%2&tk="
                     "4e17baf1ebcefc505f3121c67950cd71",
                     1.0, 0, true});
    // m_layers.append(
    //     {LayerSat,
    //      "https://webst0%1.is.autonavi.com/"
    //      "appmaptile?lang=zh_cn&size=1&scale=1&style=6&x=%2&y=%3&z=%4",
    //      1.0, 0, true});
    //  注记层：高德若有无底色style可放这；没有就用天地图注记/自绘
    m_layers.append({LayerAnno,
                     "https://t%1.tianditu.gov.cn/cva_w/"
                     "wmts?service=WMTS&request=GetTile&version=1.0.0&layer="
                     "cva&style=default&format=image/"
                     "png&tilematrixset=w&tilematrix=%4&tilerow=%3&tilecol=%2&"
                     "tk=4e17baf1ebcefc505f3121c67950cd71",
                     1.0, 1, true});
    // 天地图WMTS参数顺序和你xyz模板不同，下面load要单独处理；若用XYZ简化版见备注
  } else {
    // 路线底图
    m_layers.append(
        {LayerSat,
         "https://webrd0%1.is.autonavi.com/"
         "appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4",
         1.0, 0, true});
  }
  for (auto& s : m_loadedTiles) s.clear();
  m_scene->clear();
  ++m_epoch;
  loadVisibleTiles();
}