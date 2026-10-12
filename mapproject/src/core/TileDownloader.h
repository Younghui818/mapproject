#pragma once
#include <QAtomicInt>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QSharedPointer>
#include <QUrl>
#include <QVector>
#include <functional>

struct TileRequest {
  QUrl url;
  int x = 0;
  int y = 0;
  int z = 0;
  int layer = 0;  // 0: image, 1: dem
};
Q_DECLARE_METATYPE(TileRequest)

/**
 * @brief 单次批量下载任务的上下文，解决多次调用覆盖回调的问题
 */
class DownloadContext : public QObject {
  Q_OBJECT
 public:
  using TileCallback = std::function<void(const TileRequest&, const QByteArray&,
                                          const QString&)>;
  using DoneCallback = std::function<void()>;

  explicit DownloadContext(QNetworkAccessManager* nam,
                           QObject* parent = nullptr);

  void start(const QVector<TileRequest>& reqs, TileCallback tileCb,
             DoneCallback doneCb);

 private slots:
  void onFinished(QNetworkReply* reply);

 private:
  QNetworkAccessManager* nam_;
  TileCallback tileCb_;
  DoneCallback doneCb_;
  QAtomicInt total_;
  QAtomicInt done_;
};

class TileDownloader : public QObject {
  Q_OBJECT
 public:
  explicit TileDownloader(QObject* parent = nullptr);

  /**
   * @brief 发起一批瓦片下载（可安全并发调用多次）
   */
  void DownloadTiles(const QVector<TileRequest>& reqs,
                     DownloadContext::TileCallback tileCallback,
                     DownloadContext::DoneCallback allDone);

 signals:
  void Progress(int done, int total);

 private:
  QNetworkAccessManager* nam_;
};