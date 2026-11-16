#include "PathDataModel.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

PathDataModel::PathDataModel(QObject* parent) : QObject(parent) {}

quint64 PathDataModel::AddPath(PathData data) {
  data.name = data.name.isEmpty() ? QString("Path %1").arg(data.id) : data.name;
  paths_map_[data.id] = data;
  return data.id;
}

void PathDataModel::RemovePath(quint64 id) {
  for (auto it = paths_map_.begin(); it != paths_map_.end(); ++it) {
    if (id != it.key()) {
      continue;
    }
    paths_map_.remove(it.key());
    break;
  }
}

void PathDataModel::ClearAll() { paths_map_.clear(); }

PathData PathDataModel::GetPath(quint64 id) const {
  for (auto it = paths_map_.begin(); it != paths_map_.end(); ++it) {
    if (id == it.key()) {
      return it.value();
    }
  }
  return {};
}

void PathDataModel::SetPathStyle(quint64 id, const QColor& color, int width) {
  for (auto it = paths_map_.begin(); it != paths_map_.end(); ++it) {
    if (id == it.key()) {
      it.value().color = color;
      it.value().width = width;
      return;
    }
  }
}

double PathDataModel::ComputeLengthMeters(const QVector<PathRecord>& path) {
  if (path.size() < 2) return 0.0;
  constexpr double R = 6378137.0;
  double len = 0.0;
  for (int i = 1; i < path.size(); ++i) {
    const double dLat = (path[i].lat - path[i - 1].lat) * M_PI / 180.0;
    const double dLon = (path[i].lon - path[i - 1].lon) * M_PI / 180.0;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) + std::cos(path[i - 1].lat * M_PI / 180.0) *
                                                                   std::cos(path[i].lat * M_PI / 180.0) *
                                                                   std::sin(dLon / 2) * std::sin(dLon / 2);
    len += R * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
  }
  return len;
}

quint64 PathDataModel::GetPathNewID() {
  quint64 id = 0;
  for (auto it = paths_map_.begin(); it != paths_map_.end(); ++it) {
    if (it.key() > id) {
      id = it.key();
    }
  }
  return id++;
}

bool PathDataModel::ExportPathToFile(const QString& filePath, const QVector<quint64>& ids) const {
  if (ids.isEmpty() || filePath.isEmpty()) return false;
  QJsonArray pathsArray;

  for (quint64 id : ids) {
    if (paths_map_.count(id) <= 0) {
      continue;
    }
    const auto& points = paths_map_[id].polyline;

    QJsonObject pathObj;
    pathObj["id"] = qint64(id);
    pathObj["name"] = paths_map_[id].name;
    pathObj["color"] = paths_map_[id].color.name(QColor::HexArgb);
    pathObj["width"] = paths_map_[id].width;

    QJsonArray pointsArray;
    for (const auto& rec : points) {
      QJsonObject pt;
      pt["lat"] = rec.lat;
      pt["lon"] = rec.lon;
      pt["height"] = rec.height;
      pt["ref_lat"] = rec.ref_lat;
      pt["ref_lon"] = rec.ref_lon;
      pointsArray.append(pt);
    }
    pathObj["points"] = pointsArray;
    pathsArray.append(pathObj);
  }

  if (pathsArray.isEmpty()) return false;

  QJsonObject root;
  root["version"] = 1;
  root["paths"] = pathsArray;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "[export] cannot open file:" << filePath;
    return false;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  qDebug() << "[export] exported" << pathsArray.size() << "paths to" << filePath;
  return true;
}

int PathDataModel::ImportPathFromFile(const QString& filePath, QVector<quint64>& outIds) {
  outIds.clear();

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "[import] cannot open file:" << filePath;
    return 0;
  }

  QJsonParseError parseErr;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
  if (parseErr.error != QJsonParseError::NoError) {
    qWarning() << "[import] JSON parse error:" << parseErr.errorString();
    return 0;
  }

  const QJsonObject root = doc.object();
  const int version = root["version"].toInt(0);
  if (version != 1) {
    qWarning() << "[import] unsupported version:" << version;
    return 0;
  }

  const QJsonArray pathsArray = root["paths"].toArray();
  if (pathsArray.isEmpty()) return 0;

  int imported = 0;
  for (const auto& v : pathsArray) {
    const QJsonObject pathObj = v.toObject();

    // 解析点
    QVector<PathRecord> points;
    const QJsonArray pts = pathObj["points"].toArray();
    if (pts.size() < 2) {
      qWarning() << "[import] path skipped: less than 2 points";
      continue;
    }

    for (const auto& pv : pts) {
      const QJsonObject p = pv.toObject();
      points.append({p["lat"].toDouble(), p["lon"].toDouble(), static_cast<float>(p["height"].toDouble()),
                     p["ref_lat"].toDouble(), p["ref_lon"].toDouble()});
    }

    // 分配新 ID（避免和已有路径冲突）
    const quint64 newId = GetPathNewID();

    // 名称去重：如果已存在同名，加后缀
    QString name = pathObj["name"].toString();
    const QColor color = QColor(pathObj["color"].toString("#FF4737"));
    const int width = pathObj["width"].toInt(3);

    PathData data;
    data.polyline = points;
    data.id = newId;
    data.name = name;
    data.color = color;
    data.width = width;
  }

  qDebug() << "[import] imported" << imported << "paths";
  return imported;
}