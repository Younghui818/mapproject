#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVector>
#include <QPointF>
#include <QMetaType>

#include "PathDataModel.h"

/**
 * 一条完整路径结果
 */
struct RouteResult {
  quint64 id = 0;
  bool valid = false;
  QString provider;
  QString message;

  // 折线（经纬度）
  QVector<PathRecord> polyline;
  // 分段信息（可选）
  QVector<QString> instructions;
  QVector<double> distances;
};
Q_DECLARE_METATYPE(RouteResult)

class RouteService : public QObject {
  Q_OBJECT

 public:
  explicit RouteService(QObject* parent = nullptr);

  /**
   * 实时路径请求
   * @param points 顺序点（第1个=起点，最后=终点，中间=途经）
   */
  void RequestRoute(quint64 id, const QVector<PathRecord>& points);

  /**
   * 取消当前请求
   */
  void CancelCurrentRequest();

 signals:
  /**
   * 路由成功
   */
  void SignalRouteReady(const RouteResult& result);

  /**
   * 路由失败
   */
  void SignalRouteFailed(const QString& reason);

  /**
   * 请求被取消
   */
  void SignalRouteCancelled();

 private slots:
  void SlotReplyFinished();

 private:
  QNetworkAccessManager* nam_;
  QNetworkReply* current_reply_ = nullptr;
  quint64 epoch_ = 0;

  QString BuildTianDiTuUrl(const QVector<PathRecord>& points);
  RouteResult ParseTianDiTuResult(const QByteArray& data);
};