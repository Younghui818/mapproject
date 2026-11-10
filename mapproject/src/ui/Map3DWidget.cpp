#include "Map3DWidget.h"

#include <QMap>
#include <QPainter>
#include <QPair>
#include <QSharedPointer>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <vector>

#include "Scene3DRenderer.h"
#include "TileDownloader.h"

namespace {
// ---- 构建规模上限：限制网络请求、纹理和几何数据的最大规模 ----
constexpr int kMaxDemTiles = 64;         // 下载瓦片上限 8×8（想要更大改这个）
constexpr int kHardMaxTiles = 100;       // 绝对硬上限，防异常
constexpr int kTargetGridMax = 1024;     // 拼接后超过此分辨率就重采样
constexpr int kMaxMeshVerts = 400000;    // 网格顶点上限
constexpr int kMaxCloudPoints = 250000;  // 点云点数上限

const QString kTiandituKey = "4e17baf1ebcefc505f3121c67950cd71";

QString TdtImgUrl(int z, int x, int y) {
  // 生成天地图影像 WMTS 地址。
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
  // 生成天地图影像注记 WMTS 地址；注记瓦片以透明图层叠加。
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
  // 按请求范围创建合成图，缺失瓦片保留背景色，便于识别数据缺口。
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
  // 在源网格和目标网格之间做双线性插值，避免直接抽点造成明显台阶。
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
  // 根据 n×n 的理论点数计算抽样步长，控制 GPU 缓冲区规模。
  const long long total = 1LL * n * n;
  if (total <= maxCount) return 1;
  return std::max(1, int(std::ceil(std::sqrt(double(total) / maxCount))));
}

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

Map3DWidget::Map3DWidget(QWidget* parent)
    : QWidget(parent),
      renderer_(new Scene3DRenderer(this)),
      downloader_(new TileDownloader(this)) {
  auto* l = new QVBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->addWidget(renderer_);

  connect(renderer_, &Scene3DRenderer::SignalPathSaved, this,
          &Map3DWidget::SignalPathSaved);
  connect(renderer_, &Scene3DRenderer::SignalPathRemoved, this,
          &Map3DWidget::SignalPathRemoved);
  connect(renderer_, &Scene3DRenderer::SignalPathPointAdded, this,
          &Map3DWidget::SignalPathPointAdded);
  connect(renderer_, &Scene3DRenderer::SignalTerrainChanged, this,
          &Map3DWidget::TerrainChanged);
  connect(renderer_, &Scene3DRenderer::SignalPathStyleChanged, this,
          &Map3DWidget::PathStyleChanged);
  connect(renderer_, &Scene3DRenderer::SignalPathAnimationFinished, this,
          &Map3DWidget::SignalPathAnimationFinished);
}

Map3DWidget::~Map3DWidget() = default;

void Map3DWidget::Build3DFromCenter(double centerLat, double centerLon,
                                    double radiusMeters) {
  // 以中心点和半径计算影像/DEM 瓦片范围，随后并发下载两类数据。
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

  // ★ 正确：用 dx0/dx1/dy0/dy1/demZ
  renderer_->SetTileLatLonRange(
      Scene3DRenderer::TileXToLon(dx0, demZ),
      Scene3DRenderer::TileXToLon(dx1 + 1, demZ),
      Scene3DRenderer::TileYToLat(dy1 + 1, demZ),  // 南边界
      Scene3DRenderer::TileYToLat(dy0, demZ)       // 北边界
  );

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
        TileRequest c = r;
        c.url = QUrl(TdtCiaUrl(demZ, tx, ty));
        ciaReqs.append(c);
      }

    auto applyTexture = [this, imgShared, ciaShared, dx0, dy0, dx1, dy1]() {
      last_img_shared_ = imgShared;
      last_cia_shared_ = ciaShared;
      last_dx0_ = dx0;
      last_dy0_ = dy0;
      last_dx1_ = dx1;
      last_dy1_ = dy1;
      QImage tex = StitchImagery(*imgShared, *ciaShared, dx0, dy0, dx1, dy1,
                                 type_ == kMap3DShowTypeTexture);
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
  // 垂直夸张只影响渲染坐标，不修改原始 DEM 数值。
  if (renderer_) renderer_->SetVerticalExaggeration(k);
}

void Map3DWidget::UpdateMap3DShowType(Map3DShowType type) {
  // UI 选择点云/网格后转发给渲染器，并记录当前模式。
  // 如果时点云和网格之间的切换则重建 如果是网格和贴图之间的切换
  // 则仅更新贴图注记信息
  if ((type_ == kMap3DShowTypePoint) != (type == kMap3DShowTypePoint)) {
    if (!renderer_) {
      return;
    }
    renderer_->SetShowType(type);
    type_ = type;
    return;
  }

  type_ = type;
  if (type != kMap3DShowTypePoint) {
    if (!last_img_shared_.isNull() && !last_cia_shared_.isNull()) {
      QImage tex = StitchImagery(*last_img_shared_, *last_cia_shared_,
                                 last_dx0_, last_dy0_, last_dx1_, last_dy1_,
                                 type == kMap3DShowTypeTexture);
      if (!tex.isNull()) {
        renderer_->SetTerrainTexture(tex);
      }
    }
  }
}

void Map3DWidget::Build3DFromView(double centerLat, double centerLon,
                                  double metersPerPixel, int viewportW,
                                  int viewportH) {
  // 用视口对角线估算用户当前可见的地面范围，避免固定半径导致覆盖不足或请求过多。
  if (renderer_) renderer_->SetGeoOrigin(centerLat, centerLon);
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

void Map3DWidget::SlotSetAddingPath(bool enable) {
  // 将路径编辑状态转发给 3D 渲染器，并由渲染器负责拾取地形点。
  if (renderer_) renderer_->SetAddingPath(enable);
}

quint64 Map3DWidget::SlotSaveCurrentPath(const QString& name) {
  return renderer_ ? renderer_->SaveCurrentPath(name) : 0;
}

void Map3DWidget::SlotRemoveSavedPathById(quint64 id) {
  if (renderer_) renderer_->RemoveSavedPathById(id);
}

void Map3DWidget::SlotClearAllPaths() {
  if (renderer_) renderer_->ClearAllPaths();
}

void Map3DWidget::SetPathStyle(int width, const QColor& color) {
  if (renderer_) renderer_->SetPathStyle(width, color);
}

void Map3DWidget::SetAllPathsStyle(int width, const QColor& color) {
  if (renderer_) renderer_->SetAllPathsStyle(width, color);
}

void Map3DWidget::PlayPathAnimation(int savedPathIndex) {
  if (renderer_) renderer_->PlayPathAnimation(savedPathIndex);
}

int Map3DWidget::GetPathIndexById(quint64 id) const {
  if (!renderer_) return -1;
  const auto& ids = renderer_->GetSavedPathIds();  // 需要加 getter
  for (int i = 0; i < ids.size(); ++i)
    if (ids[i] == id) return i;
  return -1;
}

bool Map3DWidget::ExportPaths(const QVector<quint64>& ids,
                              const QString& filePath, ExportFormat format) {
  if (!renderer_) return false;
  return renderer_->ExportPaths(ids, filePath, format);
}

int Map3DWidget::ImportPaths(const QString& filePath,
                             QVector<quint64>& outIds) {
  if (!renderer_) return 0;
  return renderer_->ImportPaths(filePath, outIds);
}