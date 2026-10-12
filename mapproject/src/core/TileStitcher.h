#pragma once
#include <QObject>
#include <QImage>
#include <QVector>

class TileStitcher : public QObject {
  Q_OBJECT
public:
  explicit TileStitcher(QObject* parent = nullptr);
  // tiles: 行优先或列优先由调用方保证顺序，tilesX/tilesY 为瓦片列行数
  QImage StitchImages(const QVector<QImage>& tiles, int tilesX, int tilesY);
  QImage ResizeIfNeeded(const QImage& src, int maxSize);
};