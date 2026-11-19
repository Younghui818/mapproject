#include "RouteService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QUrlQuery>
#include <QXmlStreamReader>

const QString tk = "4e17baf1ebcefc505f3121c67950cd71";
static const QNetworkRequest::Attribute kAttrEpoch = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);
static const QNetworkRequest::Attribute kAttrRequestId =
    static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2);

RouteService::RouteService(QObject* parent) : QObject(parent), nam_(new QNetworkAccessManager(this)) {}

void RouteService::RequestRoute(quint64 id, const QVector<PathRecord>& points) {
  if (points.size() < 2) {
    emit SignalRouteFailed("need less two point");
    return;
  }

  CancelCurrentRequest();
  ++epoch_;

  QString url = BuildTianDiTuUrl(points);
  if (url.isEmpty()) {
    emit SignalRouteFailed("build url failed");
    return;
  }

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 MapStudio/1.0");

  req.setAttribute(kAttrRequestId, QVariant::fromValue(id));

  current_reply_ = nam_->get(req);
  current_reply_->setProperty("epoch", epoch_);

  connect(current_reply_, &QNetworkReply::finished, this, &RouteService::SlotReplyFinished);
}

void RouteService::CancelCurrentRequest() {
  if (current_reply_) {
    current_reply_->abort();
    current_reply_->deleteLater();
    current_reply_ = nullptr;
    emit SignalRouteCancelled();
  }
}

void RouteService::SlotReplyFinished() {
  if (!current_reply_) return;

  auto* reply = current_reply_;
  current_reply_ = nullptr;

  reply->deleteLater();

  if (reply->property("epoch").toULongLong() != epoch_) return;
  if (reply->error() == QNetworkReply::OperationCanceledError) return;

  quint64 requestId = reply->request().attribute(kAttrRequestId).toULongLong();

  if (reply->error() != QNetworkReply::NoError) {
    emit SignalRouteFailed(reply->errorString());
    return;
  }

  QByteArray data = reply->readAll();
  RouteResult result = ParseTianDiTuResult(data);
  result.id = requestId;

  if (result.valid)
    emit SignalRouteReady(result);
  else
    emit SignalRouteFailed(result.message);
}

QString RouteService::BuildTianDiTuUrl(const QVector<PathRecord>& points) {
  if (points.size() < 2) return {};

  auto toStr = [](double lon, double lat) { return QString("%1,%2").arg(lon, 0, 'f', 6).arg(lat, 0, 'f', 6); };

  QString orig = toStr(points.first().lon, points.first().lat);
  QString dest = toStr(points.last().lon, points.last().lat);

  // 中间点（不包含首尾）
  QStringList vias;
  for (int i = 1; i < points.size() - 1; ++i) vias << toStr(points[i].lon, points[i].lat);

  QJsonObject obj;
  obj["orig"] = orig;
  obj["dest"] = dest;
  obj["style"] = "3";                                // 3=步行（drive 接口里凑步行用）
  if (!vias.isEmpty()) obj["mid"] = vias.join(";");  // 分号分隔

  QString postStr = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));

  QUrl url("https://api.tianditu.gov.cn/drive");
  QUrlQuery q;
  q.addQueryItem("postStr", postStr);
  q.addQueryItem("type", "search");
  q.addQueryItem("tk", tk);
  url.setQuery(q);

  return url.toString();  // QUrl 自动做百分号编码
}

RouteResult RouteService::ParseTianDiTuResult(const QByteArray& data) {
  RouteResult result;
  result.valid = false;

  QString text = QString::fromUtf8(data).trimmed();
  if (text.isEmpty()) {
    result.message = "reply null";
    return result;
  }

  // 1. 从 XML 里抠出 routelatlon 文本
  QString routeLatLon;
  QXmlStreamReader xml(text);
  while (!xml.atEnd() && !xml.hasError()) {
    QXmlStreamReader::TokenType token = xml.readNext();
    if (token == QXmlStreamReader::StartElement && xml.name().compare("routelatlon", Qt::CaseInsensitive) == 0) {
      routeLatLon = xml.readElementText().trimmed();
      break;  // 拿到全量串就走
    }
  }

  // 兜底：万一没有 routelatlon 标签，退化到老逻辑（按分号最多串）
  if (routeLatLon.isEmpty()) {
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    int maxSc = 10;
    for (const QString& line : lines) {
      QString t = line.trimmed();
      int sc = t.count(';');
      if (sc > maxSc && t.contains(',')) {
        maxSc = sc;
        routeLatLon = t;
      }
    }
  }

  if (routeLatLon.isEmpty()) {
    result.message = "no routelatlon found";
    qDebug() << "[Route] raw:" << text.left(200);
    return result;
  }

  // 2. 解析坐标对
  QStringList segs = routeLatLon.split(';', Qt::SkipEmptyParts);
  QVector<PathRecord> rawPoints;
  rawPoints.reserve(segs.size());

  for (const QString& seg : segs) {
    QString t = seg.trimmed();
    if (t.isEmpty()) continue;
    QStringList ll = t.split(',', Qt::SkipEmptyParts);
    if (ll.size() == 2) {
      bool ok1 = false, ok2 = false;
      double lon = ll[0].toDouble(&ok1);
      double lat = ll[1].toDouble(&ok2);
      if (!ok1 || !ok2) continue;
      if (lon < 73 || lon > 135 || lat < 18 || lat > 54) continue;
      PathRecord r;
      r.lon = lon;
      r.lat = lat;
      rawPoints.append(r);
    }
  }

  // 3. 轻量去重（距离 < ~1米）
  const double MIN_DIST_SQ = 1e-8;
  QVector<PathRecord> deduped;
  deduped.reserve(rawPoints.size());
  for (const PathRecord& p : rawPoints) {
    if (!deduped.isEmpty()) {
      const PathRecord& last = deduped.last();
      double dlon = p.lon - last.lon;
      double dlat = p.lat - last.lat;
      if (dlon * dlon + dlat * dlat < MIN_DIST_SQ) continue;
    }
    deduped.append(p);
  }

  // 4. 首尾冗余裁剪（routelatlon 偶尔头尾多一个吸附点）
  while (deduped.size() > 2) {
    const PathRecord& a = deduped.first();
    const PathRecord& b = deduped[1];
    double dlon = b.lon - a.lon, dlat = b.lat - a.lat;
    if (dlon * dlon + dlat * dlat < MIN_DIST_SQ)
      deduped.removeFirst();
    else
      break;
  }
  while (deduped.size() > 2) {
    const PathRecord& a = deduped[deduped.size() - 2];
    const PathRecord& b = deduped.last();
    double dlon = b.lon - a.lon, dlat = b.lat - a.lat;
    if (dlon * dlon + dlat * dlat < MIN_DIST_SQ)
      deduped.removeLast();
    else
      break;
  }

  result.polyline = deduped;

  if (result.polyline.size() >= 2) {
    result.valid = true;
    result.provider = "tianditu";
    qDebug() << "[Route] parsed" << result.polyline.size() << "points";
    qDebug() << "[Route] first:" << result.polyline.first().lon << result.polyline.first().lat;
    qDebug() << "[Route] last:" << result.polyline.last().lon << result.polyline.last().lat;
  } else {
    result.message = QString("polyline too short: %1").arg(result.polyline.size());
  }

  return result;
}