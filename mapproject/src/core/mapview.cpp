#include "mapview.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QImageReader>
#include <QNetworkProxy>
#include <QScrollBar>
#include <cmath>
#include <QPainterPath>
#include <QTransform>

MapView::MapView(QWidget* parent)
    : QGraphicsView(parent),
      scene_(new QGraphicsScene(this)),
      nam_(new QNetworkAccessManager(this)),
      zoom_(12),
      mouse_dragging_(false),
      path_pen_(QPen(Qt::red, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)),
      marker_brush_(QBrush(Qt::red)) {
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

  QPoint hz = LatLonToTile(30.2741, 120.1551, zoom_);
  QPointF centerScene((hz.x() + 0.5) * 256.0, (hz.y() + 0.5) * 256.0);

  SetSceneRectForZoom(zoom_);
  centerOn(centerScene);
  SlotLoadVisibleTiles();
  qDebug() << "[Init map view center] center:x" << centerScene.x()
           << " y:" << centerScene.y();
}

void MapView::wheelEvent(QWheelEvent* event) {
  QPointF oldCenter = mapToScene(viewport()->rect().center());
  int oldZoom = zoom_;

  if (event->angleDelta().y() > 0 && zoom_ < 18)
    zoom_++;
  else if (event->angleDelta().y() < 0 && zoom_ > 3)
    zoom_--;
  else
    return;

  double factor = std::pow(2.0, zoom_ - oldZoom);

  // 仅清除瓦片
  ClearTileItems();
  loaded_tiles_.clear();
  ++epoch_;

  // 缩放路径（标记使用 ItemIgnoresTransformations，不缩放大小，只移动位置）
  ScalePaths(factor);

  SetSceneRectForZoom(zoom_);
  QPointF newCenter(oldCenter.x() * factor, oldCenter.y() * factor);
  centerOn(newCenter);

  SlotLoadVisibleTiles();
  scroll_timer_.stop();
}

void MapView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    press_pos_ = event->pos();
    potential_click_ = true;

    if (adding_path_) {
      // 先不立即添加，等 mouseRelease 确认是点击（未拖动）
      return;
    } else {
      mouse_dragging_ = true;
      last_mouse_pos_ = event->pos();
      setCursor(Qt::OpenHandCursor);
    }
  }
  QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event) {
  if (adding_path_ && potential_click_) {
    // 判断是否超过阈值，若超过则认为是拖拽
    QPoint delta = event->pos() - press_pos_;
    if (std::abs(delta.x()) > click_threshold_ || std::abs(delta.y()) > click_threshold_) {
      potential_click_ = false;
      // 启用拖拽行为（以便用户拖动地图）
      mouse_dragging_ = true;
      last_mouse_pos_ = event->pos();
      setCursor(Qt::OpenHandCursor);
    }
  }

  if (mouse_dragging_) {
    QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
  }

  QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    if (adding_path_) {
      if (potential_click_) {
        // 被判定为点击（非拖拽）——添加点（mapToScene 以当前位置）
        QPointF scenePt = mapToScene(event->pos());
        AddPointToCurrentPath(scenePt);
      }
      potential_click_ = false;
      // 如果在拖拽中，mouse_dragging_ 会在下面被清除
    }

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
    loaded_tiles_[layerId].remove(key);
    reply->deleteLater();
    return;
  }
  QImage img;
  if (r.read(&img) && !img.isNull()) {
    auto* item = new QGraphicsPixmapItem(QPixmap::fromImage(img));
    item->setPos(key.x() * 256.0, key.y() * 256.0);
    item->setData(0, QVariant(true)); // 标识为瓦片
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
  const double tileSize = 256.0;

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
          url = ly.url_template.arg(sub).arg(x).arg(y).arg(zoom_);
        } else {
          url = ly.url_template.arg(sub).arg(x).arg(y).arg(zoom_);
        }

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 MapStudio/1.0");
        if (url.contains("tianditu")) {
          req.setRawHeader("Referer", "https://www.tianditu.gov.cn/");
        } else if (url.contains("autonavi") || url.contains("amap")) {
          req.setRawHeader("Referer", "https://www.amap.com/");
        } else {
          req.setRawHeader("Referer", "https://mapstudio.local/");
        }
        req.setAttribute(QNetworkRequest::User, QVariant::fromValue(key));
        req.setAttribute(EpochAttr(), QVariant::fromValue(epoch_));
        req.setAttribute(layer_attr_, int(ly.id));
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

  // 仅清除瓦片项，不移除路径/标记
  ClearTileItems();
  loaded_tiles_.clear();
  ++epoch_;
  SlotLoadVisibleTiles();
}

void MapView::AddPointToCurrentPath(const QPointF& pt) {
  current_path_points_.append(pt);

  QPainterPath path;
  if (!current_path_points_.isEmpty()) {
    path.moveTo(current_path_points_.first());
    for (int i = 1; i < current_path_points_.size(); ++i) {
      path.lineTo(current_path_points_.at(i));
    }
  }

  if (!current_path_item_) {
    current_path_item_ = new QGraphicsPathItem(path);
    current_path_item_->setPen(path_pen_);
    current_path_item_->setZValue(2);
    scene_->addItem(current_path_item_);
  } else {
    current_path_item_->setPath(path);
  }

  // 标记：创建忽略变换的 marker，使其在屏幕上保持恒定像素大小
  auto* marker = new QGraphicsEllipseItem(-marker_pixel_radius_, -marker_pixel_radius_,
                                          marker_pixel_radius_ * 2, marker_pixel_radius_ * 2);
  marker->setBrush(marker_brush_);
  marker->setPen(Qt::NoPen);
  marker->setZValue(3);
  marker->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
  // 将 marker 的位置设为路径点位置（pos 是 scene 坐标）
  marker->setPos(pt);
  scene_->addItem(marker);
  current_path_markers_.append(marker);
}

void MapView::ClearTileItems() {
  QList<QGraphicsItem*> items = scene_->items();
  for (QGraphicsItem* it : items) {
    QVariant v = it->data(0);
    if (v.isValid() && v.toBool()) {
      scene_->removeItem(it);
      delete it;
    }
  }
}

void MapView::ScalePaths(double factor) {
  if (qFuzzyCompare(factor, 1.0)) return;

  // 缩放已保存路径与移动保存标记的位置（标记忽略变换，仅更新 pos）
  for (auto* pItem : saved_path_items_) {
    if (!pItem) continue;
    QPainterPath p = pItem->path();
    QTransform t;
    t.scale(factor, factor);
    pItem->setPath(t.map(p));
  }
  for (int pi = 0; pi < saved_path_markers_.size(); ++pi) {
    auto& markers = saved_path_markers_[pi];
    for (auto* m : markers) {
      if (!m) continue;
      QPointF p = m->pos();
      m->setPos(p * factor);
    }
  }

  // 当前路径
  if (current_path_item_) {
    QPainterPath p = current_path_item_->path();
    QTransform t;
    t.scale(factor, factor);
    current_path_item_->setPath(t.map(p));
  }
  for (auto* m : current_path_markers_) {
    if (!m) continue;
    QPointF p = m->pos();
    m->setPos(p * factor);
  }

  // 更新存储点（scene 坐标）
  for (int i = 0; i < current_path_points_.size(); ++i) current_path_points_[i] *= factor;
  for (auto& list : saved_path_items_) {
    Q_UNUSED(list);
  }
  // Note: saved path points are embedded in QPainterPath; we updated path above.
}

quint64 MapView::SaveCurrentPath(const QString& name) {
  if (!current_path_item_ || current_path_points_.isEmpty()) return 0;

  quint64 id = next_path_id_++;
  saved_path_items_.append(current_path_item_);
  saved_path_markers_.append(current_path_markers_);
  saved_path_ids_.append(id);
  saved_path_names_.append(name.isEmpty() ? QString("Path %1").arg(id) : name);

  // 清空 current
  current_path_item_ = nullptr;
  current_path_markers_.clear();
  current_path_points_.clear();

  emit PathSaved(id, saved_path_names_.last());
  return id;
}

void MapView::RemoveSavedPathById(quint64 id) {
  int idx = saved_path_ids_.indexOf(id);
  if (idx < 0) return;
  // remove path item
  auto* p = saved_path_items_.at(idx);
  if (p) {
    scene_->removeItem(p);
    delete p;
  }
  // remove markers
  for (auto* m : saved_path_markers_.at(idx)) {
    if (m) {
      scene_->removeItem(m);
      delete m;
    }
  }

  saved_path_items_.removeAt(idx);
  saved_path_markers_.removeAt(idx);
  saved_path_names_.removeAt(idx);
  saved_path_ids_.removeAt(idx);

  emit PathRemoved(id);
}

void MapView::SetAddingPath(bool enable) {
  adding_path_ = enable;
  potential_click_ = false;
  if (enable) {
    // 清除当前未保存的 path（保留已保存）
    if (current_path_item_) {
      scene_->removeItem(current_path_item_);
      delete current_path_item_;
      current_path_item_ = nullptr;
    }
    for (auto* m : current_path_markers_) {
      if (m) {
        scene_->removeItem(m);
        delete m;
      }
    }
    current_path_markers_.clear();
    current_path_points_.clear();
  } else {
    // 不自动保存，留给 SaveCurrentPath 由 UI 决定保存（也可直接保存）
  }
}

void MapView::ClearAllPaths() {
  // 已保存路径
  for (auto* it : saved_path_items_) {
    if (it) {
      scene_->removeItem(it);
      delete it;
    }
  }
  saved_path_items_.clear();

  for (auto& markers : saved_path_markers_) {
    for (auto* m : markers) {
      if (m) {
        scene_->removeItem(m);
        delete m;
      }
    }
  }
  saved_path_markers_.clear();
  saved_path_names_.clear();
  saved_path_ids_.clear();

  // 当前路径
  if (current_path_item_) {
    scene_->removeItem(current_path_item_);
    delete current_path_item_;
    current_path_item_ = nullptr;
  }
  for (auto* m : current_path_markers_) {
    if (m) {
      scene_->removeItem(m);
      delete m;
    }
  }
  current_path_markers_.clear();
  current_path_points_.clear();
}