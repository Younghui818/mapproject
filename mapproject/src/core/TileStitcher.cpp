#include "TileStitcher.h"
#include <QPainter>

TileStitcher::TileStitcher(QObject* parent) : QObject(parent) {}

QImage TileStitcher::StitchImages(const QVector<QImage>& tiles, int tilesX, int tilesY) {
  if (tilesX <= 0 || tilesY <= 0) return QImage();
  const int tileSize = tiles.isEmpty() ? 256 : tiles.first().width();
  QImage out(tileSize * tilesX, tileSize * tilesY, QImage::Format_RGBA8888);
  out.fill(Qt::transparent);
  QPainter p(&out);
  int idx = 0;
  for (int ty = 0; ty < tilesY; ++ty) {
    for (int tx = 0; tx < tilesX; ++tx) {
      if (idx < tiles.size() && !tiles[idx].isNull()) {
        p.drawImage(tx * tileSize, ty * tileSize, tiles[idx]);
      }
      ++idx;
    }
  }
  p.end();
  return out;
}

QImage TileStitcher::ResizeIfNeeded(const QImage& src, int maxSize) {
  if (src.isNull()) return {};
  int w = src.width();
  int h = src.height();
  if (w <= maxSize && h <= maxSize) return src;
  double sf = double(maxSize) / double(std::max(w, h));
  return src.scaled(int(w * sf), int(h * sf), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}