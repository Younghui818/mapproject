#include "Map3DWidget.h"

#include <QMap>
#include <QPainter>
#include <QPair>
#include <QSharedPointer>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "DemProcessor.h"
#include "MeshBuilder.h"
#include "Scene3DRenderer.h"
#include "TileDownloader.h"
#include "TileStitcher.h"

namespace {
// ---- 规模上限：按需调整 ----
constexpr int kMaxDemTiles = 64;         // 下载瓦片上限 8×8（想要更大改这个）
constexpr int kHardMaxTiles = 100;       // 绝对硬上限，防异常
constexpr int kTargetGridMax = 1024;     // 拼接后超过此分辨率就重采样
constexpr int kMaxMeshVerts = 400000;    // 网格顶点上限
constexpr int kMaxCloudPoints = 250000;  // 点云点数上限

const QString kTiandituKey = "4e17baf1ebcefc505f3121c67950cd71";

QString TdtImgUrl(int z, int x, int y) {
  return QString(
             "https://t0.tianditu.gov.cn/img_w/wmts"
             "?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0"
             "&LAYER=img&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles"
             "&TILEMATRIX=%1&TILEROW=%2&TILECOL=%3&tk=%4")
      .arg(z + 0)
      .arg(y)
      .arg(x)
      .arg(kTiandituKey);
}

// 影像注记（透明 PNG，叠在影像上层）
QString TdtCiaUrl(int z, int x, int y) {
  return QString(
             "https://t0.tianditu.gov.cn/cia_w/wmts"
             "?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0"
             "&LAYER=cia&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles"
             "&TILEMATRIX=%1&TILEROW=%2&TILECOL=%3&tk=%4")
      .arg(z)
      .arg(y)
      .arg(x)
      .arg(kTiandituKey);
}

// 影像 + 注记合成。out 必须带 alpha，否则注记透明区会变黑块
QImage StitchImagery(const QMap<QPair<int, int>, QByteArray>& imgTiles,
                     const QMap<QPair<int, int>, QByteArray>& ciaTiles, int x0,
                     int y0, int x1, int y1, bool withLabel) {
  const int nx = x1 - x0 + 1, ny = y1 - y0 + 1;
  QImage out(nx * 256, ny * 256, QImage::Format_RGBA8888);
  out.fill(QColor(70, 70, 75));  // 缺瓦片填深灰，一眼可见

  QPainter p(&out);
  int ok = 0;
  for (int ty = y0; ty <= y1; ++ty) {
    for (int tx = x0; tx <= x1; ++tx) {
      const auto key = qMakePair(tx, ty);
      QImage base;
      if (!base.loadFromData(imgTiles.value(key))) continue;
      const int ox = (tx - x0) * 256, oy = (ty - y0) * 256;

      p.drawImage(ox, oy,
                  base.convertToFormat(QImage::Format_RGBA8888)
                      .scaled(256, 256, Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation));

      if (withLabel) {  // QPainter 自动做 alpha 混合
        QImage lbl;
        if (lbl.loadFromData(ciaTiles.value(key)))
          p.drawImage(ox, oy,
                      lbl.scaled(256, 256, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation));
      }
      ok++;
    }
  }
  qDebug() << "[img] stitched" << out.width() << "x" << out.height() << "ok"
           << ok << "/" << (nx * ny) << "label" << withLabel;
  return ok > 0 ? out : QImage();
}

// 双线性重采样（高程数据用双线性比最邻近平滑，不会出现台阶）
void ResampleGrid(const std::vector<float>& src, int sc, int sr,
                  std::vector<float>& dst, int dc, int dr) {
  dst.assign(size_t(dc) * dr, 0.0f);
  if (src.empty() || sc < 2 || sr < 2 || dc < 2 || dr < 2) return;
  for (int r = 0; r < dr; ++r) {
    const float fy = float(r) * float(sr - 1) / float(dr - 1);
    const int y0 = int(fy);
    const int y1 = std::min(y0 + 1, sr - 1);
    const float ty = fy - y0;
    for (int c = 0; c < dc; ++c) {
      const float fx = float(c) * float(sc - 1) / float(dc - 1);
      const int x0 = int(fx);
      const int x1 = std::min(x0 + 1, sc - 1);
      const float tx = fx - x0;
      const float a = src[size_t(y0) * sc + x0];
      const float b = src[size_t(y0) * sc + x1];
      const float d = src[size_t(y1) * sc + x0];
      const float e = src[size_t(y1) * sc + x1];
      dst[size_t(r) * dc + c] =
          (a * (1 - tx) + b * tx) * (1 - ty) + (d * (1 - tx) + e * tx) * ty;
    }
  }
}

// 按上限自适应抽稀步长
int AutoStep(int n, int maxCount) {
  const long long total = 1LL * n * n;
  if (total <= maxCount) return 1;
  return std::max(1, int(std::ceil(std::sqrt(double(total) / maxCount))));
}

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

struct DemDecode {
  std::vector<float> h;
  int w = 0, hgt = 0;
  float minH = 0, maxH = 0;
  bool ok = false;
};

struct DemGrid {
  std::vector<float> h;
  int cols = 0, rows = 0;
  double widthMeters = 0, depthMeters = 0;
  float minH = 0, maxH = 0;
  int missing = 0;
  bool ok = false;
};

inline float terrariumElev(QRgb p) {
  return (qRed(p) * 256.0f + qGreen(p) + qBlue(p) / 256.0f) - 32768.0f;
}

// Terrarium: elev = (R*256 + G + B/256) - 32768
DemDecode DecodeTerrarium(const QByteArray& png) {
  DemDecode d;
  QImage img;
  if (png.isEmpty() || !img.loadFromData(png)) {
    qWarning() << "[dem] load failed, bytes:" << png.size();
    return d;
  }
  img = img.convertToFormat(QImage::Format_RGB32);
  d.w = img.width();
  d.hgt = img.height();
  d.h.resize(size_t(d.w) * d.hgt);
  float mn = 1e30f, mx = -1e30f;
  for (int y = 0; y < d.hgt; ++y)
    for (int x = 0; x < d.w; ++x) {
      const QRgb p = img.pixel(x, y);
      const float e =
          (qRed(p) * 256.0f + qGreen(p) + qBlue(p) / 256.0f) - 32768.0f;
      d.h[size_t(y) * d.w + x] = e;
      mn = std::min(mn, e);
      mx = std::max(mx, e);
    }
  d.minH = mn;
  d.maxH = mx;
  d.ok = true;
  return d;
}

// 多瓦片拼接：按行列摆进一张大 grid
DemGrid StitchTerrarium(const QMap<QPair<int, int>, QByteArray>& tiles, int x0,
                        int y0, int x1, int y1) {
  DemGrid g;
  const int nx = x1 - x0 + 1, ny = y1 - y0 + 1;
  g.cols = nx * 256;
  g.rows = ny * 256;
  g.h.assign(size_t(g.cols) * g.rows, 0.0f);

  double sum = 0.0;
  qint64 cnt = 0;
  std::vector<char> filled(size_t(nx) * ny, 0);

  for (int ty = y0; ty <= y1; ++ty) {
    for (int tx = x0; tx <= x1; ++tx) {
      const int slot = (ty - y0) * nx + (tx - x0);
      const QByteArray ba = tiles.value(qMakePair(tx, ty));
      QImage img;
      if (ba.isEmpty() || !img.loadFromData(ba)) {
        g.missing++;
        continue;
      }
      img = img.convertToFormat(QImage::Format_RGB32);

      const int ox = (tx - x0) * 256, oy = (ty - y0) * 256;
      for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) {
          const float e = terrariumElev(img.pixel(x, y));
          g.h[size_t(oy + y) * g.cols + (ox + x)] = e;
          sum += e;
          cnt++;
        }
      filled[slot] = 1;
    }
  }

  // 失败瓦片用已成功区域的均值填充，避免出现假深谷
  if (g.missing > 0 && cnt > 0) {
    const float mean = float(sum / double(cnt));
    for (int ty = y0; ty <= y1; ++ty)
      for (int tx = x0; tx <= x1; ++tx) {
        if (filled[size_t(ty - y0) * nx + (tx - x0)]) continue;
        const int ox = (tx - x0) * 256, oy = (ty - y0) * 256;
        for (int y = 0; y < 256; ++y)
          for (int x = 0; x < 256; ++x)
            g.h[size_t(oy + y) * g.cols + (ox + x)] = mean;
      }
  }

  // 稳健化：按 1%/99% 分位裁剪，避免个别异常值压扁色带
  if (cnt > 0) {
    std::vector<float> s(g.h.begin(), g.h.end());
    std::sort(s.begin(), s.end());
    const size_t lo = size_t(s.size() * 0.01), hi = size_t(s.size() * 0.99);
    const float vlo = s[lo], vhi = s[std::min(hi, s.size() - 1)];
    for (auto& v : g.h) v = std::clamp(v, vlo, vhi);
    g.minH = vlo;
    g.maxH = vhi;
  }
  g.ok = (cnt > 0);
  return g;
}
}  // namespace

// 简化：选固定 zoom 值（可改为根据 radius 计算）
static int ChooseImageZoom(double /*radiusMeters*/) { return 11; }

static QPoint LonLatToTileXY(double lat, double lon, int zoom) {
  double latRad = lat * M_PI / 180.0;
  int n = 1 << zoom;
  int x = static_cast<int>(std::floor((lon + 180.0) / 360.0 * n));
  int y = static_cast<int>(std::floor(
      (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 *
      n));
  return QPoint(x, y);
}

// 按目标跨度反算 zoom，使总像素落在 ~800px，并限制瓦片数量不超过 4×4
static int ChooseDemZoom(double radiusMeters, double centerLat,
                         double centerLon, int* nxOut = nullptr,
                         int* nyOut = nullptr) {
  const double equatorRes = 40075016.686 / 256.0;  // zoom0 赤道分辨率 m/px
  const double cosLat = std::max(0.05, std::cos(centerLat * M_PI / 180.0));
  const double targetRes = (2.0 * radiusMeters) / 800.0;

  int z = std::clamp(int(std::round(std::log2((equatorRes * cosLat) /
                                              std::max(1e-6, targetRes)))),
                     4, 14);

  // 用 centerLat / centerLon 推导瓦片跨度
  auto tileSpan = [&](int zz, int& nx, int& ny) {
    const double latD = (radiusMeters / 6371000.0) * 180.0 / M_PI;
    const double lonD = (radiusMeters / (6371000.0 * cosLat)) * 180.0 / M_PI;
    const QPoint a = LonLatToTileXY(centerLat + latD, centerLon - lonD, zz);
    const QPoint b = LonLatToTileXY(centerLat - latD, centerLon + lonD, zz);
    nx = std::abs(b.x() - a.x()) + 1;
    ny = std::abs(b.y() - a.y()) + 1;
  };

  int nx = 1, ny = 1;
  tileSpan(z, nx, ny);
  while (z > 4 && nx * ny > kMaxDemTiles) {
    --z;
    tileSpan(z, nx, ny);
  }
  if (nxOut) *nxOut = nx;
  if (nyOut) *nyOut = ny;
  return z;
}

// Web Mercator 分辨率 (m/px)
static double MercatorRes(double lat, int zoom) {
  return 156543.03392804097 * std::cos(lat * M_PI / 180.0) / double(1 << zoom);
}

static double VisibleRadiusMeters(double centerLat, int zoom, int viewportW,
                                  int viewportH, double cover = 1.15) {
  const double res = MercatorRes(centerLat, zoom);  // 米/像素
  const double halfMin = std::min(viewportW, viewportH) * 0.5;
  return std::max(300.0, halfMin * res * cover);  // 下限 300m，防 zoom 过大
}

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
  constexpr bool kAlsoFetchImagery = true;  // 贴图阶段再打开

  int nx = 1, ny = 1;
  const int demZ = ChooseDemZoom(radiusMeters, centerLat, centerLon, &nx, &ny);

  constexpr double R = 6371000.0;
  const double latDelta = (radiusMeters / R) * 180.0 / M_PI;
  const double lonDelta =
      (radiusMeters /
       (R * std::max(0.05, std::cos(centerLat * M_PI / 180.0)))) *
      180.0 / M_PI;

  const QPoint d00 =
      LonLatToTileXY(centerLat + latDelta, centerLon - lonDelta, demZ);
  const QPoint d11 =
      LonLatToTileXY(centerLat - latDelta, centerLon + lonDelta, demZ);
  int dx0 = qMin(d00.x(), d11.x()), dx1 = qMax(d00.x(), d11.x());
  int dy0 = qMin(d00.y(), d11.y()), dy1 = qMax(d00.y(), d11.y());

  // 保险：硬上限 4×4
  if ((dx1 - dx0 + 1) * (dy1 - dy0 + 1) > kMaxDemTiles) {
    const int cx = (dx0 + dx1) / 2, cy = (dy0 + dy1) / 2;
    const int half = int(std::sqrt(double(kMaxDemTiles))) / 2;
    dx0 = cx - half;
    dx1 = cx + half - 1;
    dy0 = cy - half;
    dy1 = cy + half - 1;
  }
  if ((dx1 - dx0 + 1) * (dy1 - dy0 + 1) > kHardMaxTiles) {
    qWarning() << "[dem] tile count exceeds hard limit, abort";
    return;
  }
  const int cntX = dx1 - dx0 + 1, cntY = dy1 - dy0 + 1;
  qDebug() << "[dem] z" << demZ << "tiles" << cntX << "x" << cntY << "range x["
           << dx0 << "," << dx1 << "] y[" << dy0 << "," << dy1 << "]";

  QVector<TileRequest> demReqs;
  for (int ty = dy0; ty <= dy1; ++ty)
    for (int tx = dx0; tx <= dx1; ++tx) {
      TileRequest r;
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

  auto shared = QSharedPointer<QMap<QPair<int, int>, QByteArray>>::create();
  const double mpp = MercatorRes(centerLat, demZ);  // 米/像素

  downloader_->DownloadTiles(
      demReqs,
      [shared](const TileRequest& tr, const QByteArray& data,
               const QString& err) {
        if (data.isEmpty()) {
          qWarning() << "[dem] fail" << tr.x << tr.y << err;
          return;
        }
        shared->insert(qMakePair(tr.x, tr.y), data);
      },
      [this, shared, dx0, dy0, dx1, dy1, mpp]() {
        DemGrid g = StitchTerrarium(*shared, dx0, dy0, dx1, dy1);
        if (!g.ok) {
          qWarning() << "[dem] no tiles decoded";
          return;
        }

        // ★ 地面尺寸必须在重采样【之前】算：用原始分辨率 × 米/像素
        //   重采样只降分辨率，不改变地理范围
        g.widthMeters = g.cols * mpp;
        g.depthMeters = g.rows * mpp;
        qDebug() << "[dem] grid" << g.cols << "x" << g.rows << "ground"
                 << int(g.widthMeters) << "x" << int(g.depthMeters)
                 << "m  relief" << g.minH << "→" << g.maxH << "missing"
                 << g.missing;

        // ---- 超分辨率就重采样（地理范围不变）----
        if (std::max(g.cols, g.rows) > kTargetGridMax) {
          const double k =
              double(kTargetGridMax) / double(std::max(g.cols, g.rows));
          const int nc = std::max(2, int(g.cols * k));
          const int nr = std::max(2, int(g.rows * k));
          std::vector<float> rs;
          ResampleGrid(g.h, g.cols, g.rows, rs, nc, nr);
          qDebug() << "[dem] resample" << g.cols << "x" << g.rows << "->" << nc
                   << "x" << nr;
          g.h = std::move(rs);
          g.cols = nc;
          g.rows = nr;
        }

        // 顺序：先喂数据，再调参数（避免重复 rebuild）
        renderer_->SetHeightGrid(g.h, g.cols, g.rows, g.widthMeters,
                                 g.depthMeters);

        const int meshStep = AutoStep(g.cols, kMaxMeshVerts);
        const int pointStep = AutoStep(g.cols, kMaxCloudPoints);
        renderer_->SetVerticalExaggeration(std::clamp(
            g.widthMeters / (std::max(1.0, double(g.maxH - g.minH)) * 25.0),
            1.0, 8.0));
        renderer_->SetMeshStep(meshStep);
        renderer_->SetPointStep(pointStep);
        renderer_->SetPointSize(4.0f);

        qDebug() << "[dem] step mesh" << meshStep << "point" << pointStep
                 << "verts~" << (g.cols / meshStep) * (g.rows / meshStep);

        emit SignalReady();
      });

  if constexpr (kAlsoFetchImagery) {
    constexpr bool kWithLabel = true;
    auto imgShared =
        QSharedPointer<QMap<QPair<int, int>, QByteArray>>::create();
    auto ciaShared =
        QSharedPointer<QMap<QPair<int, int>, QByteArray>>::create();

    // 与 DEM 完全相同的瓦片集合（同 zoom、同 x/y 范围）
    QVector<TileRequest> imgReqs, ciaReqs;
    for (int ty = dy0; ty <= dy1; ++ty)
      for (int tx = dx0; tx <= dx1; ++tx) {
        TileRequest r;
        r.x = tx;
        r.y = ty;
        r.z = demZ;
        r.layer = 0;
        r.url = QUrl(TdtImgUrl(demZ, tx, ty));
        imgReqs.append(r);
        if constexpr (kWithLabel) {
          TileRequest c = r;
          c.url = QUrl(TdtCiaUrl(demZ, tx, ty));
          ciaReqs.append(c);
        }
      }

    auto applyTexture = [this, imgShared, ciaShared, dx0, dy0, dx1, dy1]() {
      QImage tex =
          StitchImagery(*imgShared, *ciaShared, dx0, dy0, dx1, dy1, kWithLabel);
      if (tex.isNull()) {
        qWarning() << "[img] no imagery";
        return;
      }
      renderer_->SetTerrainTexture(tex);
      qDebug() << "[img] texture applied";
    };

    downloader_->DownloadTiles(
        imgReqs,
        [imgShared](const TileRequest& tr, const QByteArray& d,
                    const QString& e) {
          if (d.isEmpty()) {
            qWarning() << "[img] fail" << tr.x << tr.y << e;
            return;
          }
          imgShared->insert(qMakePair(tr.x, tr.y), d);
        },
        [this, ciaShared, ciaReqs, applyTexture]() {
          if (ciaReqs.isEmpty()) {
            applyTexture();
            return;
          }
          downloader_->DownloadTiles(
              ciaReqs,
              [ciaShared](const TileRequest& tr, const QByteArray& d,
                          const QString&) {
                if (!d.isEmpty()) ciaShared->insert(qMakePair(tr.x, tr.y), d);
              },
              applyTexture);
        });
  }
}

void Map3DWidget::SetDemExaggeration(double k) {
  if (renderer_) renderer_->SetVerticalExaggeration(k);
}

void Map3DWidget::UpdateMap3dShowType(Map3DShowType type) {
  if (!renderer_) {
    return;
  }
  renderer_->SetShowType(type);
}

void Map3DWidget::Show3DImageDemo() {
  renderer_->SetPointSize(5.0f);
  renderer_->SetMeshStep(2);  // 128×128 网格
  renderer_->SetHeightData(MakeDemoTerrain(256),
                           /*width=*/200.0, /*depth=*/200.0,
                           /*maxHeight=*/45.0);

  return;
}

void Map3DWidget::Build3DFromDemTile(double centerLat, double centerLon,
                                     int zoom) {
  const QPoint t = LonLatToTileXY(centerLat, centerLon, zoom);
  const double metersPerTile =
      40075016.686 * std::cos(centerLat * M_PI / 180.0) / double(1 << zoom);

  TileRequest r;
  r.url = QUrl(QString("https://s3.amazonaws.com/elevation-tiles-prod/"
                       "terrarium/%1/%2/%3.png")
                   .arg(zoom)
                   .arg(t.x())
                   .arg(t.y()));
  r.x = t.x();
  r.y = t.y();
  r.z = zoom;
  r.layer = 1;

  qDebug() << "[dem] request z" << zoom << "tile(" << t.x() << "," << t.y()
           << ") ground" << metersPerTile << "m";

  downloader_->DownloadTiles(
      QVector<TileRequest>{r},
      [this, metersPerTile](const TileRequest& tr, const QByteArray& data,
                            const QString& err) {
        if (data.isEmpty()) {
          qWarning() << "[dem] empty:" << err << tr.url.toString();
          return;
        }
        qDebug() << "[dem] bytes:" << data.size();
        DemDecode d = DecodeTerrarium(data);
        if (!d.ok) {
          qWarning() << "[dem] decode failed";
          return;
        }
        qDebug() << "[dem]" << d.w << "x" << d.hgt << "min" << d.minH << "max"
                 << d.maxH << "relief" << (d.maxH - d.minH) << "m";

        renderer_->SetHeightGrid(d.h, d.w, d.hgt, metersPerTile, metersPerTile);
        renderer_->SetVerticalExaggeration(2.0);
        renderer_->SetMeshStep(1);
        renderer_->SetPointSize(4.0f);

        emit SignalReady();
      },
      []() { qDebug() << "[dem] group done"; });
}

void Map3DWidget::Build3DFromView(double centerLat, double centerLon,
                                  double metersPerPixel, int viewportW,
                                  int viewportH) {
  constexpr double kCover = 1.15;  // 比 2D 视野多抓一圈，边缘不露白
  const double halfMin = std::min(viewportW, viewportH) * 0.5;
  const double radius = std::max(300.0, halfMin * metersPerPixel * kCover);

  qDebug() << "[3d-entry] lat" << centerLat << "lon" << centerLon << "mpp"
           << metersPerPixel << "view" << viewportW << "x" << viewportH
           << "=> radius" << int(radius) << "m";

  Build3DFromCenter(centerLat, centerLon, radius);
}

void Map3DWidget::SetAmbient(float v) {
  if (renderer_) renderer_->SetAmbient(v);
}

void Map3DWidget::SetLightAzimuth(float deg) {
  if (renderer_) renderer_->SetLightAzimuth(deg);
}

void Map3DWidget::SetLightElevation(float deg) {
  if (renderer_) renderer_->SetLightElevation(deg);
}

void Map3DWidget::SetBrightness(float v) {
  if (renderer_) renderer_->SetBrightness(v);
}

void Map3DWidget::SetContrast(float v) {
  if (renderer_) renderer_->SetContrast(v);
}

void Map3DWidget::ApplyLightPreset(int idx) {
  if (renderer_) renderer_->ApplyLightPreset(idx);
}
