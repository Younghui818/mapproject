#pragma once

#include <QImage>
#include <QVector3D>
#include <QVector>
#include <QWidget>
#include <Qt3DCore/QEntity>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointSize>
#include <vector>

#include "MeshBuilder.h"

enum Map3DShowType {
  kMap3DShowTypePoint = 0,  // 点云
  kMap3DShowTypeMesh,       // 网格
  kMap3DShowTypeTexture     // 贴图（预留）
};

class Scene3DRenderer : public QWidget {
  Q_OBJECT
 public:
  explicit Scene3DRenderer(QWidget* parent = nullptr);
  void SetMesh(const MeshData& mesh);

  // ---- 数据入口 ----
  void SetHeightData(const QImage& heightMap, double width, double depth,
                     double maxHeight);  // 灰度图（demo）
  void SetHeightGrid(const std::vector<float>& heights, int cols, int rows,
                     double width, double depth);  // 真实高程（米）

  // ---- 显示切换 ----
  void SetShowType(Map3DShowType type);
  Map3DShowType ShowType() const { return showType_; }

  void SetPointSize(float px);
  void SetPointStep(int step);
  void SetMeshStep(int step);
  void SetWireframe(bool on);
  void SetHillshade(bool on);
  void SetShadeLight(float x, float y, float z);
  void SetAmbient(float v);
  void SetVerticalExaggeration(double k);

  double HeightMin() const { return heightMin_; }
  double HeightMax() const { return heightMax_; }
  void ClearAll();

 signals:
  void ClickedAt(double x, double y, double z);

 private:
  void setupScene();
  void rebuild();
  void destroyEntity(Qt3DCore::QEntity*& e);
  void frameCamera();
  void applyPointSize();
  void makeEntity(Qt3DCore::QEntity*& ent, const void* geo, bool asPoints,
                  bool wireframe);

  Qt3DExtras::Qt3DWindow* view_ = nullptr;
  QWidget* container_ = nullptr;
  Qt3DCore::QEntity* rootEntity_ = nullptr;
  Qt3DRender::QCamera* camera_ = nullptr;
  Qt3DExtras::QOrbitCameraController* camController_ = nullptr;

  Qt3DCore::QEntity* pointCloudEntity_ = nullptr;
  Qt3DCore::QEntity* meshEntity_ = nullptr;
  QVector<Qt3DRender::QPointSize*> pointSizeNodes_;
  float pointSize_ = 4.0f;

  std::vector<float> grid_;  // 真实高程（米），行优先
  int gridCols_ = 0, gridRows_ = 0;
  bool hasData_ = false;
  double heightMin_ = 0.0, heightMax_ = 0.0;

  double width_ = 100.0, depth_ = 100.0;
  double exaggeration_ = 1.0;

  Map3DShowType showType_ = kMap3DShowTypePoint;
  int pointStep_ = 2;
  int meshStep_ = 1;
  bool wireframe_ = false;
  bool hillshade_ = true;
  QVector3D lightDir_ = QVector3D(0.45f, 0.78f, 0.44f).normalized();
  float ambient_ = 0.35f;
};