#include "mapview.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QImageReader>
#include <QNetworkProxy>
#include <QPainterPath>
#include <QScrollBar>
#include <QTransform>
#include <cmath>

const QString kTianDiTuKey = "4e17baf1ebcefc505f3121c67950cd71";

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
  connect(nam_, &QNetworkAccessManager::finished, this, &MapView::SlotOnTileLoaded);

  scroll_timer_.setSingleShot(true);
  connect(&scroll_timer_, &QTimer::timeout, this, &MapView::SlotLoadVisibleTiles);
  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { scroll_timer_.start(150); });
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { scroll_timer_.start(150); });

  // 初始视图定位到默认经纬度，再加载当前可见区域的瓦片。
  QPointF hz = LatLonToScene(30.2741, 120.1551, zoom_);

  SetSceneRectForZoom(zoom_);
  centerOn(hz);
  SlotLoadVisibleTiles();
  qDebug() << "[Init map view center] center:x" << hz.x() << " y:" << hz.y();
}

void MapView::wheelEvent(QWheelEvent* event) {
  // 以鼠标滚轮改变缩放级别；瓦片清空后重新请求可见区域，路径按同一比例缩放。
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
  // 路径模式下先记录按下位置，等 release 时确认是点击还是地图拖拽。
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
  // 超过点击阈值后切换为拖拽，并通过滚动条移动地图视口。
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
  // 释放左键时完成路径加点或结束本次地图拖拽。
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
  // 校验请求时代编号，丢弃缩放/切图后返回的过期响应，再解码并放入场景。
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
    item->setData(0, QVariant(true));  // 标识为瓦片
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
  // 根据视口在场景中的范围计算瓦片行列，过滤边界后提交尚未加载的请求。
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

QPointF MapView::LatLonToScene(double lat, double lon, int zoom) {
  constexpr double PI = M_PI;
  double latRad = lat * PI / 180.0;
  int n = 1 << zoom;

  double x = (lon + 180.0) / 360.0 * n;
  double y = (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / PI) / 2.0 * n;

  return QPointF(x * 256.0, y * 256.0);
}

void MapView::SetSceneRectForZoom(int zoom) {
  double world = 256.0 * (1 << zoom);
  scene_->setSceneRect(0, 0, world, world);
}

void MapView::SwitchTileServer(MapSources sources, ImageType type, bool show_cva) {
  layers_.clear();
  switch (sources) {
    case kMapSourcesAMap:
      if (type == kImageTypeSt) {
        // 卫星层
        layers_.append({LayerSat,
                        "https://webst0%1.is.autonavi.com/"
                        "appmaptile?lang=zh_cn&size=1&scale=1&style=6&x=%2&y=%3&z=%4",
                        1.0, 0, true});
        if (show_cva) {
          layers_.append({LayerAnno,
                          "https://webst0%1.is.autonavi.com/"
                          "appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4",
                          1.0, 1, true});
        }
      } else {
        // 路线底图
        if (show_cva) {
          layers_.append({LayerSat,
                          "https://webrd0%1.is.autonavi.com/"
                          "appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4",
                          1.0, 0, true});
        } else {
          layers_.append({LayerSat,
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
                        "TILEMATRIX=%4&TILEROW=%3&TILECOL=%2&tk=" +
                            kTianDiTuKey,
                        1.0, 0, true});
      } else {
        // 路线底图
        layers_.append({LayerSat,
                        "https://t%1.tianditu.gov.cn/vec_w/wmts?"
                        "SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=vec&"
                        "STYLE=default&FORMAT=tiles&TILEMATRIXSET=w&"
                        "TILEMATRIX=%4&TILEROW=%3&TILECOL=%2&tk=" +
                            kTianDiTuKey,
                        1.0, 0, true});
      }
      if (show_cva) {
        layers_.append({LayerAnno,
                        "https://t%1.tianditu.gov.cn/cva_w/"
                        "wmts?service=WMTS&request=GetTile&version=1.0.0&layer="
                        "cva&style=default&format=image/"
                        "png&tilematrixset=w&tilematrix=%4&tilerow=%3&tilecol=%2&"
                        "tk=" +
                            kTianDiTuKey,
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
  current_path_data_.id = next_path_id_;
  double lat = 0.0, lon = 0.0;
  if (!SceneToLatLon(pt, lat, lon)) {
    return;
  }
  PathRecord record;
  record.lat = lat;
  record.lon = lon;
  current_path_data_.points.append(record);
  emit SignalPathAddPoint();  // 通知 UI 更新当前路径点数显示
}

void MapView::ScalePaths(double factor) {
  if (qFuzzyCompare(factor, 1.0)) return;

  // 缩放已保存路径与移动保存标记的位置（标记忽略变换，仅更新 pos）
  for (auto it = saved_path_items_map_.begin(); it != saved_path_items_map_.end(); ++it) {
    quint64 id = it.key();
    QGraphicsPathItem* item = it.value();
    if (item) {
      QPainterPath p = item->path();
      QTransform t;
      t.scale(factor, factor);
      item->setPath(t.map(p));
    }
  }

  // 当前路径
  if (current_path_item_) {
    QPainterPath p = current_path_item_->path();
    QTransform t;
    t.scale(factor, factor);
    current_path_item_->setPath(t.map(p));
  }
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

quint64 MapView::SlotSaveCurrentPath(const QString& name) {
  if (!current_path_item_) return 0;

  quint64 id = next_path_id_++;
  saved_path_items_map_.insert(id, current_path_item_);
  emit SignalPathSaved(current_path_data_);
  // 清空 current
  current_path_item_ = nullptr;
  current_path_data_.points.clear();
  return id;
}

void MapView::SlotRemoveSavedPathById(quint64 id) {
  // remove path item
  for (auto it = saved_path_items_map_.begin(); it != saved_path_items_map_.end(); ++it) {
    if (it.key() != id) {
      continue;
    }
    QGraphicsPathItem* item = it.value();
    if (item) {
      scene_->removeItem(item);
      delete item;
    }
  }
  saved_path_items_map_.remove(id);
  emit SignalPathRemoved(id);
}

void MapView::SlotSetAddingPath(bool enable) {
  adding_path_ = enable;
  potential_click_ = false;
  if (enable) {
    // 清除当前未保存的 path（保留已保存）
    if (current_path_item_) {
      scene_->removeItem(current_path_item_);
      delete current_path_item_;
      current_path_item_ = nullptr;
    }
    current_path_data_.points.clear();
  } else {
    // 不自动保存，留给 SaveCurrentPath 由 UI 决定保存（也可直接保存）
  }
}

void MapView::SlotClearAllPaths() {
  // 已保存路径
  for (auto it = saved_path_items_map_.begin(); it != saved_path_items_map_.end(); ++it) {
    quint64 id = it.key();
    QGraphicsPathItem* item = it.value();
    if (item) {
      scene_->removeItem(item);
      delete item;
    }
  }
  saved_path_items_map_.clear();

  // 当前路径
  if (current_path_item_) {
    scene_->removeItem(current_path_item_);
    delete current_path_item_;
    current_path_item_ = nullptr;
  }
  current_path_data_.points.clear();
}

// 在文件适当位置添加 SceneToLatLon 实现
bool MapView::SceneToLatLon(const QPointF& scenePt, double& outLat, double& outLon) const {
  // 计算 tile x/y（浮点）
  double tx = scenePt.x() / 256.0;
  double ty = scenePt.y() / 256.0;
  int z = zoom_;
  double n = static_cast<double>(1 << z);
  double lon = tx / n * 360.0 - 180.0;

  // 反 WebMercator Y -> lat
  double y = ty / n;
  double lat_rad = atan(sinh(M_PI * (1.0 - 2.0 * y)));
  double lat = lat_rad * 180.0 / M_PI;

  outLat = lat;
  outLon = lon;
  return true;
}

void MapView::RefreshPathPen() {
  path_pen_.setColor(current_path_data_.color);
  path_pen_.setWidth(current_path_data_.width);
  // 视图未做缩放变换（缩放走的是 ScalePaths 改几何），cosmetic 与否等价；
  // 加上可保证将来若使用 view->scale()，线宽仍按屏幕像素显示
  path_pen_.setCosmetic(true);
}

void MapView::SetPathStyle(int width, const QColor& color) {
  current_path_data_.width = qMax(1, width);
  current_path_data_.color = color;
  RefreshPathPen();

  // 正在绘制的路径 + 它的标记
  if (current_path_item_) current_path_item_->setPen(path_pen_);
  viewport()->update();
}

void MapView::SetAllPathsStyle(int width, const QColor& color) {
  SetPathStyle(width, color);

  // 已保存路径
  for (auto it = saved_path_items_map_.begin(); it != saved_path_items_map_.end(); ++it) {
    quint64 id = it.key();
    QGraphicsPathItem* item = it.value();
    if (item) {
      item->setPen(path_pen_);
      qDebug() << "path id:" << id << "pos:" << item->pos() << "visible:" << item->isVisible();
    }
  }
  viewport()->update();
}

void MapView::PaintPath(PathData path_data) {
  if (path_data.points.size() < 1) {
    return;
  }

  for (auto it = saved_path_items_map_.begin(); it != saved_path_items_map_.end(); ++it) {
    quint64 id = it.key();
    if (id != path_data.id) {
      continue;
    }
    QGraphicsPathItem* item = it.value();
    if (item) {
      scene_->removeItem(item);
      delete item;
    }
    saved_path_items_map_.remove(id);
    break;
  }

  QPainterPath path;
  if (path_data.polyline.size() == 1) {
    const PathRecord& rec = path_data.polyline[0];
    // 经纬度 → tile坐标 → scene坐标
    QPointF scenePt = LatLonToScene(rec.lat, rec.lon, zoom_);
    // 固定方向（水平向右）
    double angle = 0.0;  // 弧度，0 = 向右
    double halfLen = path_data.width / 2.0;
    QPointF p1 = scenePt + QPointF(-halfLen, 0);
    QPointF p2 = scenePt + QPointF(halfLen, 0);

    path.moveTo(p1);
    path.lineTo(p2);
  } else {
    for (int i = 0; i < path_data.polyline.size(); ++i) {
      const PathRecord& rec = path_data.polyline[i];
      // 经纬度 → tile坐标 → scene坐标
      QPointF scenePt = LatLonToScene(rec.lat, rec.lon, zoom_);
      if (i == 0)
        path.moveTo(scenePt);
      else
        path.lineTo(scenePt);
    }
  }

  // 创建路径项
  auto* item = new QGraphicsPathItem(path);
  QPen pen;
  pen.setColor(path_data.color);
  pen.setWidth(path_data.width);
  pen.setCosmetic(true);  // 缩放时线宽不变
  item->setPen(pen);
  item->setZValue(3);  // 在瓦片之上

  // 标记为用户数据（非瓦片）
  item->setData(0, QVariant(false));
  scene_->addItem(item);
  saved_path_items_map_[path_data.id] = item;
  return;
}

void MapView::RefreshPath(QVector<PathData> path_datas) {
  // 绘制新路径
  for (const auto& path_data : path_datas) {
    PaintPath(path_data);
  }
}