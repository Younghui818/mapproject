#pragma once
#include <QObject>
#include <QVector>
#include <QByteArray>
#include <vector>

class DemProcessor : public QObject {
  Q_OBJECT
public:
  explicit DemProcessor(QObject* parent = nullptr);

  struct HeightGrid {
    int width = 0;
    int height = 0;
    std::vector<float> heights; // row-major
  };

  // demTiles按行列顺序提供（PNG terrarium bytes），tilesX/Y 表示瓦片格局
  HeightGrid DecodeAndResample(const QVector<QByteArray>& demTiles, int tilesX, int tilesY, int tileSize, double targetResolutionMeters);
};