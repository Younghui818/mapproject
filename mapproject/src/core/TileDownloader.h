#pragma once
#include <QAtomicInt>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>
#include <QVector>
#include <functional>

struct TileRequest {
  QUrl url;       // 瓦片请求地址
  int x = 0;      // Web Mercator 瓦片 X
  int y = 0;      // Web Mercator 瓦片 Y
  int z = 0;      // 缩放级别
  int layer = 0;  // 图层类型：0 为影像，1 为 DEM
};
Q_DECLARE_METATYPE(TileRequest)

/**
 * @brief 单次批量下载任务的上下文。
 * 每次批量请求拥有独立回调和计数器，避免并发调用时相互覆盖状态。
 */
class DownloadContext : public QObject {
  Q_OBJECT
 public:
  // 单张瓦片完成回调：参数依次为请求元数据、响应字节和错误文本；成功时错误文本为空。
  using TileCallback = std::function<void(const TileRequest&, const QByteArray&,
                                          const QString&)>;
  // 全部请求处理结束后调用一次；空请求列表也会立即完成。
  using DoneCallback = std::function<void()>;

  // nam 必须在本上下文存活期间有效；parent 决定该上下文的 Qt 对象所有权。
  explicit DownloadContext(QNetworkAccessManager* nam,
                           QObject* parent = nullptr);

  // 启动一批异步请求，并在本上下文中保存本批次的回调和完成计数。
  void start(const QVector<TileRequest>& reqs, TileCallback tileCb,
             DoneCallback doneCb);

 private slots:
  // 处理单个网络请求完成事件，并推进本批次完成计数。
  void onFinished(QNetworkReply* reply);

 private:
  QNetworkAccessManager* nam_;  // 由 TileDownloader 持有的网络管理器
  TileCallback tileCb_;         // 单张瓦片完成后的回调
  DoneCallback doneCb_;         // 全部瓦片完成后的回调
  QAtomicInt total_;            // 本批次总请求数
  QAtomicInt done_;             // 已完成请求数
};

class TileDownloader : public QObject {
  Q_OBJECT
 public:
  // 创建网络管理器，并注册 TileRequest 以供 QVariant/信号槽传递。
  explicit TileDownloader(QObject* parent = nullptr);

  // 发起一批瓦片下载；每次调用使用独立 DownloadContext，可安全并发。
  void DownloadTiles(const QVector<TileRequest>& reqs,
                     DownloadContext::TileCallback tileCallback,
                     DownloadContext::DoneCallback allDone);

 signals:
  // 下载进度通知；当前实现保留该信号供进度 UI 使用。
  void Progress(int done, int total);

 private:
  // 所有批次共享的网络管理器，由 QObject 父子关系负责释放。
  QNetworkAccessManager* nam_;
};