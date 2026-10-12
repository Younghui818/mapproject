#include "DemProcessor.h"
#include <QImage>
#include <QBuffer>
#include <cmath>
#include <QPainter>

// Terrarium decoding：height = (R * 256 + G + B/256) - 32768
static float TerrariumHeightFromRGB(const QRgb& px) {
  int r = qRed(px);
  int g = qGreen(px);
  int b = qBlue(px);
  float h = (r * 256.0f + g + b / 256.0f) - 32768.0f;
  return h;
}

DemProcessor::DemProcessor(QObject* parent) : QObject(parent) {}

DemProcessor::HeightGrid DemProcessor::DecodeAndResample(const QVector<QByteArray>& demTiles, int tilesX, int tilesY, int tileSize, double targetResolutionMeters) {
  HeightGrid out;
  if (tilesX <= 0 || tilesY <= 0 || demTiles.isEmpty()) return out;

  // 简单实现：把所有 dem 瓦片拼接为大图（每瓦片为 tileSize, 假定顺序与 TileStitcher 一致）
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

  // 现在解码像素到高度数组（row-major）
  out.width = fullW;
  out.height = fullH;
  out.heights.resize(out.width * out.height);
  for (int y = 0; y < fullH; ++y) {
    for (int x = 0; x < fullW; ++x) {
      const QRgb* line = reinterpret_cast<const QRgb*>(big.constScanLine(y));
      out.heights[y * out.width + x] = TerrariumHeightFromRGB(line[x]);
    }
  }

  // 重采样到目标分辨率：简单实现为下采样/上采样到目标格距对应的像素数
  // 这里不转换米->像素，调用方应保证 targetResolutionMeters 与当前 dem 分辨率一致或简单近似
  // For prototype return assembled grid.
  return out;
}