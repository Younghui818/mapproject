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
      scene_(new QGraphicsScene(this)),
      nam_(new QNetworkAccessManager(this)),
      zoom_(12),
      mouse_dragging_(false) {
  setRenderHint(QPainter::SmoothPixmapTransform);
  setBackgroundBrush(QBrush(QColor(191, 191, 191)));
  setScene(scene_);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QNetworkProxyFactory::setUseSystemConfiguration(true);

  connect(nam_, &QNetworkAccessManager::finished, this,
          &MapView::SlotOnTileLoaded);

  scroll_timer_.setSingleShot(true);
  connect(&scroll_timer_, &QTimer::timeout, this,
          &MapView::SlotLoadVisibleTiles);
  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
          [this] { scroll_timer_.start(150); });
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this] { scroll_timer_.start(150); });

  int z = zoom_;
  QPoint hz = LatLonToTile(30.2741, 120.1551, z);
  QPointF centerScene((hz.x() + 0.5) * 256.0, (hz.y() + 0.5) * 256.0);

  SetSceneRectForZoom(z);  // 整世界场景矩形
  centerOn(centerScene);
  SlotLoadVisibleTiles();
  qDebug() << "[Init map view center] center:x" << centerScene.x()
           << " y:" << centerScene.y();
}

void MapView::wheelEvent(QWheelEvent* event) {
  // 记住当前场景中心（zoom 改变后需要换算）
  QPointF oldCenter = mapToScene(viewport()->rect().center());
  qDebug() << "[Scale map view old center] center:x" << oldCenter.x()
           << " y:" << oldCenter.y();
  int oldZoom = zoom_;

  if (event->angleDelta().y() > 0 && zoom_ < 18)
    zoom_++;
  else if (event->angleDelta().y() < 0 && zoom_ > 3)
    zoom_--;
  else {
    return;
  }

  for (auto& s : loaded_tiles_) s.clear();
  scene_->clear();
  ++epoch_;
  // 场景 rect 随 zoom 改变
  SetSceneRectForZoom(zoom_);

  // 场景坐标按 zoom 差值等比缩放，保持地理位置不变
  double factor = std::pow(2.0, zoom_ - oldZoom);
  QPointF newCenter(oldCenter.x() * factor, oldCenter.y() * factor);
  centerOn(newCenter);
  qDebug() << "[Scale map view new center] center:x" << newCenter.x()
           << " y:" << newCenter.y();
  SlotLoadVisibleTiles();
  scroll_timer_.stop();
}

void MapView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    mouse_dragging_ = true;
    last_mouse_pos_ = event->pos();
    setCursor(Qt::OpenHandCursor);
  }
  QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event) {
  if (mouse_dragging_) {
    QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();
    // ★ 无 transform，视口像素 = 场景单位，直接累加
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
  }
  QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    mouse_dragging_ = false;
    setCursor(Qt::ArrowCursor);
  }
  QGraphicsView::mouseReleaseEvent(event);
}

void MapView::SlotOnTileLoaded(QNetworkReply* reply) {
  auto key = reply->request().attribute(QNetworkRequest::User).toPoint();
  int layerId = reply->request().attribute(layer_attr_).toInt();
  quint64 ep = reply->request().attribute(EpochAttr()).toULongLong();
  if (ep != epoch_) {
    reply->deleteLater();
    return;
  }

  int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QByteArray ctype =
      reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
  QByteArray data = reply->readAll();

  if (reply->error() != QNetworkReply::NoError) {
    loaded_tiles_[layerId].remove(key);
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
    loaded_tiles_[layerId].remove(key);
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
    for (auto& l : layers_)
      if (l.id == TileLayerId(layerId)) {
        op = l.opacity;
        z = l.zBase;
      }
    item->setOpacity(op);
    item->setZValue(z);
    scene_->addItem(item);
  } else {
    loaded_tiles_[layerId].remove(key);
  }
  reply->deleteLater();
}

void MapView::SlotLoadVisibleTiles() {
  QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();

  qDebug() << "[Get map view rect] x:" << visibleRect.x()
           << " y:" << visibleRect.y() << " width:" << visibleRect.width()
           << " height:" << visibleRect.height();

  const double tileSize = 256.0;  // 瓦片永远 256 场景单位

  int left = static_cast<int>(std::floor(visibleRect.left() / tileSize));
  int right = static_cast<int>(std::floor(visibleRect.right() / tileSize));
  int top = static_cast<int>(std::floor(visibleRect.top() / tileSize));
  int bottom = static_cast<int>(std::floor(visibleRect.bottom() / tileSize));
  int maxT = (1 << zoom_) - 1;

  for (const auto& ly : layers_) {
    if (!ly.enabled) continue;
    for (int x = left; x <= right; ++x) {
      for (int y = top; y <= bottom; ++y) {
        if (x < 0 || x > maxT || y < 0 || y > maxT) continue;
        QPoint key(x, y);
        if (loaded_tiles_[ly.id].contains(key)) continue;
        int sub = (x + y) % 4 + 1;
        QString url;
        if (ly.url_template.contains("tianditu")) {
          // 天地图WMTS示例：col=x,row=y,matrix=zoom
          url = ly.url_template.arg(sub).arg(x).arg(y).arg(
              zoom_);  // 按你模板占位调整
        } else {
          url = ly.url_template.arg(sub).arg(x).arg(y).arg(zoom_);
        }

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 MapStudio/1.0");
        if (url.contains("tianditu")) {
          req.setRawHeader("Referer", "https://www.tianditu.gov.cn/");
        } else if (url.contains("autonavi") || url.contains("amap")) {
          req.setRawHeader("Referer", "https://www.amap.com/");
        } else {
          req.setRawHeader("Referer", "https://mapstudio.local/");
        }
        req.setAttribute(QNetworkRequest::User, QVariant::fromValue(key));
        req.setAttribute(EpochAttr(), QVariant::fromValue(epoch_));
        req.setAttribute(layer_attr_,
                         int(ly.id));  // 自定义attr存layer
        nam_->get(req);
        loaded_tiles_[ly.id].insert(key);
      }
    }
  }
}

QPoint MapView::LatLonToTile(double lat, double lon, int zoom) {
  constexpr double PI = 3.14159265358979323846;
  double latRad = lat * PI / 180.0;
  int n = 1 << zoom;
  int x = static_cast<int>((lon + 180.0) / 360.0 * n);
  int y = static_cast<int>(
      (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / PI) / 2.0 *
      n);
  return QPoint(x, y);
}

void MapView::SetSceneRectForZoom(int zoom) {
  double world = 256.0 * (1 << zoom);
  scene_->setSceneRect(0, 0, world, world);
}

void MapView::SwitchTileServer(MapSources sources, ImageType type,
                               bool show_cva) {
  layers_.clear();
  switch (sources) {
    case kMapSourcesAMap:
      if (type == kImageTypeSt) {
        // 卫星层
        layers_.append(
            {LayerSat,
             "https://webst0%1.is.autonavi.com/"
             "appmaptile?lang=zh_cn&size=1&scale=1&style=6&x=%2&y=%3&z=%4",
             1.0, 0, true});
        if (show_cva) {
          layers_.append(
              {LayerAnno,
               "https://webst0%1.is.autonavi.com/"
               "appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4",
               1.0, 1, true});
        }
      } else {
        // 路线底图
        if (show_cva) {
          layers_.append(
              {LayerSat,
               "https://webrd0%1.is.autonavi.com/"
               "appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4",
               1.0, 0, true});
        } else {
          layers_.append(
              {LayerSat,
               "https://wprd0%1.is.autonavi.com/"
               "appmaptile?lang=zh_cn&size=1&scale=1&style=7&x=%2&y=%3&z=%4",
               1.0, 0, true});
        }
      }
      break;
    case kMapSourcesTianDiTu:
      if (type == kImageTypeSt) {
        // 卫星层
        layers_.append({LayerSat,
                        "https://t%1.tianditu.gov.cn/img_w/wmts?"
                        "SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=img&"
                        "STYLE=default&FORMAT=tiles&TILEMATRIXSET=w&"
                        "TILEMATRIX=%4&TILEROW=%3&TILECOL=%2&tk="
                        "4e17baf1ebcefc505f3121c67950cd71",
                        1.0, 0, true});
      } else {
        // 路线底图
        layers_.append({LayerSat,
                        "https://t%1.tianditu.gov.cn/vec_w/wmts?"
                        "SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=vec&"
                        "STYLE=default&FORMAT=tiles&TILEMATRIXSET=w&"
                        "TILEMATRIX=%4&TILEROW=%3&TILECOL=%2&tk="
                        "4e17baf1ebcefc505f3121c67950cd71",
                        1.0, 0, true});
      }
      if (show_cva) {
        layers_.append(
            {LayerAnno,
             "https://t%1.tianditu.gov.cn/cva_w/"
             "wmts?service=WMTS&request=GetTile&version=1.0.0&layer="
             "cva&style=default&format=image/"
             "png&tilematrixset=w&tilematrix=%4&tilerow=%3&tilecol=%2&"
             "tk=4e17baf1ebcefc505f3121c67950cd71",
             1.0, 1, true});
      }

      break;
    default:
      break;
  }
  for (auto& s : loaded_tiles_) s.clear();
  scene_->clear();
  ++epoch_;
  SlotLoadVisibleTiles();
}