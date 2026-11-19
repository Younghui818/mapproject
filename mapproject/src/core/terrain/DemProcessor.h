#pragma once
#include <QByteArray>
#include <QObject>
#include <QVector>
#include <vector>

// DEM（数字高程模型）解码器：把 Terrarium 编码的 PNG 瓦片转换为高程网格。
class DemProcessor : public QObject {
  Q_OBJECT
 public:
  // parent 用于接入 Qt 对象树，未指定时由调用方管理对象生命周期。
  explicit DemProcessor(QObject* parent = nullptr);

  // 解码后的高程栅格；宽高分别表示列数和行数。
  struct HeightGrid {
    int width = 0;               // 网格列数
    int height = 0;              // 网格行数
    std::vector<float> heights;  // 按行优先存储，索引为 y * width + x
  };

  // demTiles 按行优先提供 PNG Terrarium 数据，tilesX/tilesY 表示瓦片列数/行数。
  // 当前实现负责拼接和像素解码；targetResolutionMeters
  // 暂作为后续重采样扩展参数保留。
  // 返回的高度数据为空时表示输入瓦片范围无效或没有可解码的数据。
  HeightGrid DecodeAndResample(const QVector<QByteArray>& demTiles, int tilesX,
                               int tilesY, int tileSize,
                               double targetResolutionMeters);
};