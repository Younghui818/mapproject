#include "Map3DWidget.h"

#include <QVBoxLayout>
#include <QtMath>
#include <cmath>
#include <random>
#include <vector>

#include "DemProcessor.h"
#include "MeshBuilder.h"
#include "Scene3DRenderer.h"
#include "TileDownloader.h"
#include "TileStitcher.h"

// 简化：选固定 zoom 值（可改为根据 radius 计算）
static int ChooseImageZoom(double /*radiusMeters*/) { return 11; }
static int ChooseDemZoom(double /*radiusMeters*/) { return 13; }

static QPoint LonLatToTileXY(double lat, double lon, int zoom) {
  double latRad = lat * M_PI / 180.0;
  int n = 1 << zoom;
  int x = static_cast<int>(std::floor((lon + 180.0) / 360.0 * n));
  int y = static_cast<int>(std::floor(
      (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 *
      n));
  return QPoint(x, y);
}

namespace {
// 多峰高斯 + 平滑噪声，生成有真实起伏的 demo 高程图
QImage MakeDemoTerrain(int size = 256) {
  const int nL = 16;  // 低频噪声网格
  std::vector<float> grid((nL + 1) * (nL + 1));
  std::mt19937 rng(20240610);
  std::uniform_real_distribution<float> d(0.0f, 1.0f);
  for (auto& v : grid) v = d(rng);

  auto noise = [&](float u, float v) {
    const float fx = u * nL, fy = v * nL;
    int x0 = std::min(int(fx), nL - 1), y0 = std::min(int(fy), nL - 1);
    float tx = fx - x0, ty = fy - y0;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);  // smoothstep
    const auto at = [&](int x, int y) { return grid[y * (nL + 1) + x]; };
    const float a = at(x0, y0), b = at(x0 + 1, y0);
    const float c = at(x0, y0 + 1), e = at(x0 + 1, y0 + 1);
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + e * tx) * ty;
  };

  struct Peak {
    float cx, cy, r, amp;
  };
  const Peak peaks[] = {{0.30f, 0.32f, 0.22f, 0.85f},
                        {0.68f, 0.25f, 0.16f, 0.60f},
                        {0.55f, 0.70f, 0.26f, 0.95f},
                        {0.18f, 0.78f, 0.14f, 0.45f}};

  QImage img(size, size, QImage::Format_Grayscale8);
  for (int y = 0; y < size; ++y) {
    uchar* line = img.scanLine(y);
    for (int x = 0; x < size; ++x) {
      const float u = float(x) / (size - 1), v = float(y) / (size - 1);
      float hgt = noise(u, v) * 0.30f;
      for (const auto& p : peaks) {
        const float dx = u - p.cx, dy = v - p.cy;
        hgt += p.amp * std::exp(-((dx * dx + dy * dy) / (p.r * p.r)) * 2.2f);
      }
      line[x] = static_cast<uchar>(std::clamp(hgt, 0.0f, 1.0f) * 255.0f);
    }
  }
  return img;
}
}  // namespace

Map3DWidget::Map3DWidget(QWidget* parent)
    : QWidget(parent),
      renderer_(new Scene3DRenderer(this)),
      downloader_(new TileDownloader(this)),
      stitcher_(new TileStitcher(this)),
      dem_(new DemProcessor(this)),
      meshBuilder_(new MeshBuilder(this)) {
  auto* l = new QVBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->addWidget(renderer_);
}

Map3DWidget::~Map3DWidget() = default;

void Map3DWidget::Build3DFromCenter(double centerLat, double centerLon,
                                    double radiusMeters) {
  renderer_->SetPointSize(5.0f);
  renderer_->SetMeshStep(2);  // 128×128 网格
  renderer_->SetHeightData(MakeDemoTerrain(256),
                           /*width=*/200.0, /*depth=*/200.0,
                           /*maxHeight=*/45.0);

  return;

  // 1) 选择 zoom
  int imgZ = ChooseImageZoom(radiusMeters);
  int demZ = ChooseDemZoom(radiusMeters);

  // 2) 计算 bbox（近似：经纬度差）
  constexpr double R = 6371000.0;
  double latDelta = (radiusMeters / R) * 180.0 / M_PI;
  double lonDelta =
      (radiusMeters / (R * qCos(centerLat * M_PI / 180.0))) * 180.0 / M_PI;
  double minLat = centerLat - latDelta;
  double maxLat = centerLat + latDelta;
  double minLon = centerLon - lonDelta;
  double maxLon = centerLon + lonDelta;

  // 3) 计算需要的 tile 范围（image）
  QPoint t00 =
      LonLatToTileXY(maxLat, minLon, imgZ);  // note: lat flips for tile Y
  QPoint t11 = LonLatToTileXY(minLat, maxLon, imgZ);
  int tx0 = qMin(t00.x(), t11.x());
  int tx1 = qMax(t00.x(), t11.x());
  int ty0 = qMin(t00.y(), t11.y());
  int ty1 = qMax(t00.y(), t11.y());

  QVector<TileRequest> imgReqs;
  for (int ty = ty0; ty <= ty1; ++ty) {
    for (int tx = tx0; tx <= tx1; ++tx) {
      TileRequest r;
      // 使用 天地图 作为示例底图
      r.url = QUrl(
          QString(
              "https://t0.tianditu.gov.cn/img_w/"
              "wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER="
              "img&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles&TILECOL="
              "%1&TILEROW=%2&TILEMATRIX=%3&tk=4e17baf1ebcefc505f3121c67950cd71")
              .arg(tx)
              .arg(ty)
              .arg(imgZ));
      r.x = tx;
      r.y = ty;
      r.z = imgZ;
      r.layer = 0;
      imgReqs.append(r);
    }
  }

  // 4) DEM tiles 范围（使用 demZ）
  QPoint d00 = LonLatToTileXY(maxLat, minLon, demZ);
  QPoint d11 = LonLatToTileXY(minLat, maxLon, demZ);
  int dx0 = qMin(d00.x(), d11.x());
  int dx1 = qMax(d00.x(), d11.x());
  int dy0 = qMin(d00.y(), d11.y());
  int dy1 = qMax(d00.y(), d11.y());

  QVector<TileRequest> demReqs;
  for (int ty = dy0; ty <= dy1; ++ty) {
    for (int tx = dx0; tx <= dx1; ++tx) {
      TileRequest r;
      // Terrarium
      r.url = QUrl(QString("https://s3.amazonaws.com/elevation-tiles-prod/"
                           "terrarium/%1/%2/%3.png")
                       .arg(demZ)
                       .arg(tx)
                       .arg(ty));
      r.x = tx;
      r.y = ty;
      r.z = demZ;
      r.layer = 1;
      demReqs.append(r);
    }
  }

  struct SharedData {
    QMap<QPair<int, int>, QImage> imageTiles;
    QMap<QPair<int, int>, QByteArray> demTiles;
    QAtomicInt pendingGroups{2};
    // 捕获所需的范围参数
    int tx0, ty0, tx1, ty1;
    int dx0, dy0, dx1, dy1;
  };
  auto shared = QSharedPointer<SharedData>::create();
  shared->tx0 = tx0;
  shared->ty0 = ty0;
  shared->tx1 = tx1;
  shared->ty1 = ty1;
  shared->dx0 = dx0;
  shared->dy0 = dy0;
  shared->dx1 = dx1;
  shared->dy1 = dy1;

  // 统一的完成处理（消除重复代码）
  auto onBothDone = [this, shared]() {
    // Stitch images
    QVector<QImage> imgs;
    for (int ty = shared->ty0; ty <= shared->ty1; ++ty)
      for (int tx = shared->tx0; tx <= shared->tx1; ++tx) {
        auto k = qMakePair(tx, ty);
        if (shared->imageTiles.contains(k))
          imgs.append(shared->imageTiles.value(k));
        else {
          QImage blank(256, 256, QImage::Format_RGBA8888);
          blank.fill(Qt::transparent);
          imgs.append(blank);
        }
      }
    QImage stitched = stitcher_->StitchImages(
        imgs, shared->tx1 - shared->tx0 + 1, shared->ty1 - shared->ty0 + 1);
    QImage tex = stitcher_->ResizeIfNeeded(stitched, 4096);

    // Decode DEM
    QVector<QByteArray> demData;
    for (int ty = shared->dy0; ty <= shared->dy1; ++ty)
      for (int tx = shared->dx0; tx <= shared->dx1; ++tx)
        demData.append(shared->demTiles.value(qMakePair(tx, ty)));

    DemProcessor::HeightGrid grid =
        dem_->DecodeAndResample(demData, shared->dx1 - shared->dx0 + 1,
                                shared->dy1 - shared->dy0 + 1, 256, 100.0);

    QImage heightMap(grid.width, grid.height, QImage::Format_Grayscale8);
    float maxH = 100.0f;
    for (int y = 0; y < grid.height; ++y) {
      for (int x = 0; x < grid.width; ++x) {
        float h = grid.heights[y * grid.width + x];
        uchar val = static_cast<uchar>(qBound(0.0f, h / maxH * 255.0f, 255.0f));
        heightMap.setPixel(x, y, qRgb(val, val, val));
      }
    }

    // renderer_->SetHeightMap(heightMap, 100.0, 100.0, 100.0);
    emit SignalReady();
  };

  // Image 下载
  downloader_->DownloadTiles(
      imgReqs,
      [shared](const TileRequest& tr, const QByteArray& data, const QString&) {
        auto k = qMakePair(tr.x, tr.y);
        if (!data.isEmpty()) {
          QImage img;
          img.loadFromData(data);
          if (!img.isNull()) shared->imageTiles.insert(k, img);
        }
      },
      [shared, onBothDone]() {
        if (shared->pendingGroups.fetchAndSubAcquire(1) == 1) onBothDone();
      });

  // DEM 下载
  downloader_->DownloadTiles(
      demReqs,
      [shared](const TileRequest& tr, const QByteArray& data, const QString&) {
        shared->demTiles.insert(qMakePair(tr.x, tr.y), data);
      },
      [shared, onBothDone]() {
        if (shared->pendingGroups.fetchAndSubAcquire(1) == 1) onBothDone();
      });
}

void Map3DWidget::UpdateMap3dShowType(Map3DShowType type) {
  if (!renderer_) {
    return;
  }
  renderer_->SetShowType(type);
}