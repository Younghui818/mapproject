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
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  QNetworkProxyFactory::setUseSystemConfiguration(true);

  connect(m_nam, &QNetworkAccessManager::finished, this,
          &MapView::onTileLoaded);

  m_scrollTimer.setSingleShot(true);
  connect(&m_scrollTimer, &QTimer::timeout, this, &MapView::loadVisibleTiles);
  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
          [this] { m_scrollTimer.start(150); });
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this] { m_scrollTimer.start(150); });

  setSceneRectForZoom(m_zoom);

  QPoint hz = latLonToTile(30.2741, 120.1551, m_zoom);
  centerOn((hz.x() + 0.5) * 256.0, (hz.y() + 0.5) * 256.0);
  loadVisibleTiles();
}

void MapView::wheelEvent(QWheelEvent* event) {
  // 记住当前场景中心（zoom 改变后需要换算）
  QPointF oldCenter = mapToScene(viewport()->rect().center());
  int oldZoom = m_zoom;

  if (event->angleDelta().y() > 0 && m_zoom < 18)
    m_zoom++;
  else if (event->angleDelta().y() < 0 && m_zoom > 3)
    m_zoom--;
  else {
    return;
  }

  m_scene->clear();
  m_loadedTiles.clear();
  ++m_epoch;
  // 场景 rect 随 zoom 改变
  setSceneRectForZoom(m_zoom);

  // 场景坐标按 zoom 差值等比缩放，保持地理位置不变
  double factor = std::pow(2.0, m_zoom - oldZoom);
  QPointF newCenter(oldCenter.x() * factor, oldCenter.y() * factor);
  centerOn(newCenter);

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
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta.y());
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
    m_loadedTiles.remove(key);
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
    m_loadedTiles.remove(key);
    reply->deleteLater();
    return;
  }
  QImage img;
  if (r.read(&img) && !img.isNull()) {
    auto* item = new QGraphicsPixmapItem(QPixmap::fromImage(img));
    item->setPos(key.x() * 256.0, key.y() * 256.0);
    m_scene->addItem(item);
  } else {
    qDebug() << "[tile] reader read failed, err=" << r.errorString();
    m_loadedTiles.remove(key);
  }
  reply->deleteLater();
}

void MapView::loadVisibleTiles() {
  QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();

  const double tileSize = 256.0;  // 瓦片永远 256 场景单位

  int tileLeft = static_cast<int>(std::floor(visibleRect.left() / tileSize));
  int tileRight = static_cast<int>(std::floor(visibleRect.right() / tileSize));
  int tileTop = static_cast<int>(std::floor(visibleRect.top() / tileSize));
  int tileBottom =
      static_cast<int>(std::floor(visibleRect.bottom() / tileSize));

  qDebug() << "[tiles] zoom=" << m_zoom << " range:" << tileLeft << tileRight
           << tileTop << tileBottom;

  for (int x = tileLeft; x <= tileRight; ++x) {
    for (int y = tileTop; y <= tileBottom; ++y) {
      int maxTile = (1 << m_zoom) - 1;
      if (x < 0 || x > maxTile || y < 0 || y > maxTile) continue;

      QPoint key(x, y);
      if (m_loadedTiles.contains(key)) continue;
      int sub = (x + y) % 4 + 1;
      QString url = QString(url_).arg(sub).arg(x).arg(y).arg(m_zoom);

      QNetworkRequest req(url);
      req.setHeader(QNetworkRequest::UserAgentHeader,
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) MapStudio/1.0");
      req.setRawHeader("Referer", "https://www.amap.com/");
      req.setAttribute(QNetworkRequest::User, QVariant::fromValue(key));
      req.setAttribute(epochAttr(), QVariant::fromValue<quint64>(m_epoch));
      m_nam->get(req);

      m_loadedTiles.insert(key);
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
  switch (type) {
    case kImageTypeRoads:
      url_ =
          "https://webrd0%1.is.autonavi.com/"
          "appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4";
      break;
    case kImageTypeSt:
      url_ =
          "https://webst0%1.is.autonavi.com/"
          "appmaptile?lang=zh_cn&size=1&scale=1&style=6&x=%2&y=%3&z=%4";
      break;
    default:
      break;
  }
  m_scene->clear();
  m_loadedTiles.clear();
  ++m_epoch;             
  loadVisibleTiles();
}