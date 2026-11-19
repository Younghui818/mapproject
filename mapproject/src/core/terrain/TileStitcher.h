#pragma once
#include <QImage>
#include <QObject>
#include <QVector>

// 将多张同尺寸地图瓦片按网格位置拼接成一张大图，并按需缩放结果。
class TileStitcher : public QObject {
  Q_OBJECT
 public:
  // parent 用于接入 Qt 对象树。
  explicit TileStitcher(QObject* parent = nullptr);

  // tiles 的排列顺序由调用方保证；tilesX/tilesY 分别表示列数和行数。
  // 返回图像尺寸为单瓦片尺寸乘以瓦片列数/行数；无效尺寸时返回空图像。
  QImage StitchImages(const QVector<QImage>& tiles, int tilesX, int tilesY);

  // 当任一边超过 maxSize 时按比例缩小，避免生成过大的纹理。
  // 图像尺寸未超限时原样返回；输入图像为空时返回空图像。
  QImage ResizeIfNeeded(const QImage& src, int maxSize);
};