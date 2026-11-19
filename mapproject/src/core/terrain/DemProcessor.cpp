#include "DemProcessor.h"
#include <QImage>
#include <QPainter>

// Terrarium 编码将高程拆分到 RGB 三个通道中：
// height = (R * 256 + G + B / 256) - 32768。
static float TerrariumHeightFromRGB(const QRgb& px) {
  int r = qRed(px);
  int g = qGreen(px);
  int b = qBlue(px);
  float h = (r * 256.0f + g + b / 256.0f) - 32768.0f;
  return h;
}

DemProcessor::DemProcessor(QObject* parent) : QObject(parent) {}

DemProcessor::HeightGrid DemProcessor::DecodeAndResample(const QVector<QByteArray>& demTiles, int tilesX, int tilesY, int tileSize, double targetResolutionMeters) {
  Q_UNUSED(targetResolutionMeters);

  HeightGrid out;
  if (tilesX <= 0 || tilesY <= 0 || demTiles.isEmpty()) return out;

  // 先按瓦片网格拼成一张大图。这里假定每个瓦片尺寸为 tileSize，
  // 并且输入顺序与 TileStitcher 相同，即从上到下、从左到右。
  int fullW = tilesX * tileSize;
  int fullH = tilesY * tileSize;
  QImage big(fullW, fullH, QImage::Format_RGBA8888);
  big.fill(Qt::transparent);
  int idx = 0;
  for (int ty = 0; ty < tilesY; ++ty) {
    for (int tx = 0; tx < tilesX; ++tx) {
      if (idx < demTiles.size()) {
        QImage img;
        img.loadFromData(demTiles[idx]);
        if (!img.isNull()) {
          QPainter p(&big);
          p.drawImage(tx * tileSize, ty * tileSize, img);
          p.end();
        }
      }
      ++idx;
    }
  }

  // 再把拼接图逐像素解码为行优先高程数组，供后续网格生成使用。
  out.width = fullW;
  out.height = fullH;
  out.heights.resize(out.width * out.height);
  for (int y = 0; y < fullH; ++y) {
    for (int x = 0; x < fullW; ++x) {
      const QRgb* line = reinterpret_cast<const QRgb*>(big.constScanLine(y));
      out.heights[y * out.width + x] = TerrariumHeightFromRGB(line[x]);
    }
  }

  // 当前原型尚未根据目标米制分辨率重新采样，直接返回拼接后的原始网格。
  return out;
}