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
  kMap3DShowTypePoint = 0,  // 点云显示
  kMap3DShowTypeMesh,       // 网格
  kMap3DShowTypeTexture     // 贴图
};

class Scene3DRenderer : public QWidget {
  Q_OBJECT
 public:
  explicit Scene3DRenderer(QWidget* parent = nullptr);

  // ---- 数据入口：喂一次高程数据，之后可任意切换显示方式 ----
  void SetHeightData(const QImage& heightMap, double width, double depth,
                     double maxHeight);
  // ---- 显示方式切换 ----
  void SetShowType(Map3DShowType type);
  Map3DShowType ShowType() const { return showType_; }

  void SetPointSize(float px);  // 点大小，仅点云模式生效
  void SetPointStep(int step);  // 点云抽稀步长
  void SetMeshStep(int step);   // 网格抽稀步长
  void SetWireframe(bool on);   // 网格线框开关

  void ClearAll();
  // ---- 底层接口，也可单独调用 ----
  void SetPointCloud(const QImage& heightMap, double width, double depth,
                     double maxHeight, int step = 2);
  void SetPointCloud(const std::vector<QVector3D>& points,
                     const std::vector<QVector3D>& colors = {});
  void SetHeightMesh(const QImage& heightMap, double width, double depth,
                     double maxHeight, int step = 4, bool wireframe = false);
  void ClearPointCloud();
  void ClearMesh();

 signals:
  void ClickedAt(double x, double y, double z);

 private:
  void setupScene();
  void rebuild();  // 按当前 showType_ 重建
  void destroyEntity(Qt3DCore::QEntity*& e);
  void frameCamera(double width, double depth, double maxHeight);
  void applyPointSize();  // 把 pointSize_ 同步到已注入的 QPointSize

  void SetHillshade(bool on);                     // 光照烘焙开关
  void SetShadeLight(float x, float y, float z);  // 光照方向
  void SetAmbient(float v);                       // 环境光比例 0~1
  QVector3D shade(const QVector3D& base, const QVector3D& n) const;

  Qt3DExtras::Qt3DWindow* view_ = nullptr;
  QWidget* container_ = nullptr;
  Qt3DCore::QEntity* rootEntity_ = nullptr;
  Qt3DRender::QCamera* camera_ = nullptr;
  Qt3DExtras::QOrbitCameraController* camController_ = nullptr;

  Qt3DCore::QEntity* pointCloudEntity_ = nullptr;
  Qt3DCore::QEntity* meshEntity_ = nullptr;

  QVector<Qt3DRender::QPointSize*> pointSizeNodes_;  // 供 applyPointSize 同步
  float pointSize_ = 3.0f;

  // 最近一次高程数据，切换显示方式时复用
  bool hasData_ = false;
  QImage heightMap_;
  double width_ = 0.0;
  double depth_ = 0.0;
  double maxHeight_ = 0.0;

  Map3DShowType showType_ = kMap3DShowTypePoint;
  int pointStep_ = 2;
  int meshStep_ = 4;
  bool wireframe_ = false;

  bool hillshade_ = true;
  QVector3D lightDir_ = QVector3D(0.45f, 0.78f, 0.44f).normalized();
  float ambient_ = 0.35f;
};