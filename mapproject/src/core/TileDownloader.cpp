#include "TileDownloader.h"

#include <QDebug>
#include <QNetworkRequest>

// ==================== DownloadContext ====================
DownloadContext::DownloadContext(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), nam_(nam), total_(0), done_(0) {}

void DownloadContext::start(const QVector<TileRequest>& reqs,
                            TileCallback tileCb, DoneCallback doneCb) {
  tileCb_ = std::move(tileCb);
  doneCb_ = std::move(doneCb);
  total_.storeRelease(reqs.size());
  done_.storeRelease(0);

  if (reqs.isEmpty()) {
    if (doneCb_) doneCb_();
    return;
  }

  for (const auto& r : reqs) {
    QNetworkRequest qreq(r.url);
    // 将 TileRequest 整体存入 User 属性，避免多个 attribute 拆散
    qreq.setAttribute(QNetworkRequest::User, QVariant::fromValue(r));
    QNetworkReply* reply = nam_->get(qreq);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onFinished(reply); });
  }
}

void DownloadContext::onFinished(QNetworkReply* reply) {
  if (!reply) return;
  reply->deleteLater();

  TileRequest tr =
      reply->request().attribute(QNetworkRequest::User).value<TileRequest>();
  QByteArray data = reply->readAll();
  QString err;

  if (reply->error() != QNetworkReply::NoError) {
    err = reply->errorString();
    qDebug() << "[TileDownloader] error:" << tr.x << tr.y << tr.z << err;
  }

  // 回调直接在当前线程执行（NAM 所在线程），无需 invokeMethod
  if (tileCb_) {
    tileCb_(tr, data, err);
  }

  int currentDone = done_.fetchAndAddAcquire(1) + 1;
  int currentTotal = total_.loadAcquire();

  if (currentDone >= currentTotal) {
    if (doneCb_) {
      // 确保 allDone 在所有 tile 回调之后触发
      QMetaObject::invokeMethod(
          this, [cb = doneCb_]() { cb(); }, Qt::QueuedConnection);
    }
  }
}

// ==================== TileDownloader ====================
TileDownloader::TileDownloader(QObject* parent) : QObject(parent) {
  nam_ = new QNetworkAccessManager(this);
  qRegisterMetaType<TileRequest>("TileRequest");
}

void TileDownloader::DownloadTiles(const QVector<TileRequest>& reqs,
                                   DownloadContext::TileCallback tileCallback,
                                   DownloadContext::DoneCallback allDone) {
  // 每次调用创建独立 Context，支持并发多组下载
  auto* ctx = new DownloadContext(nam_, this);
  ctx->start(reqs, std::move(tileCallback), std::move(allDone));
  // Context 在完成后可自行 deleteLater，或由父对象管理
  connect(ctx, &QObject::destroyed, this, [] {});  // placeholder
}