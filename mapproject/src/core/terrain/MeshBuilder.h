#pragma once
#include <QImage>
#include <QObject>
#include <QVector3D>
#include <vector>

#include "DemProcessor.h"

// CPU 侧生成的地形网格数据，最终会被 Scene3DRenderer 转换为 Qt 3D 几何缓冲区。
struct MeshData {
  std::vector<QVector3D> vertices;  // 顶点位置
  std::vector<QVector3D> normals;   // 与顶点一一对应的法线
  std::vector<int> indices;         // 三角形索引，每三个元素组成一个面
  QImage texture;                   // 可选的地形纹理
};

// 将规则高程网格转换为三角形网格，并计算平滑法线。
class MeshBuilder : public QObject {
  Q_OBJECT
 public:
  // parent 用于接入 Qt 对象树。
  explicit MeshBuilder(QObject* parent = nullptr);

  // 将高程栅格转换为三角网格；gridSpacingMeters 是 X/Y 相邻采样点间距（米）。
  // texture 会随结果返回，供渲染层作为可选材质使用。
  MeshData BuildMesh(const DemProcessor::HeightGrid& grid,
                     const QImage& texture, double gridSpacingMeters);
};