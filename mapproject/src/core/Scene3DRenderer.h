#pragma once
#include <QImage>
#include <QWidget>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometryRenderer>

#include "MeshBuilder.h"

class QLabel;

class Scene3DRenderer : public QWidget {
  Q_OBJECT
 public:
  explicit Scene3DRenderer(QWidget* parent = nullptr);
  void SetMesh(const MeshData& mesh);

  // ---- 新增：仅点云（不贴图）----
  void SetPointCloud(const QImage& heightMap, double width, double depth,
                     double maxHeight, int step = 2);  // 从高度图采样
  void SetPointCloud(const std::vector<QVector3D>& points,
                     const std::vector<QVector3D>& colors = {});  // 直接喂点集
  void SetPointSize(float px);
  void ClearPointCloud();

  // ---- 新增：网格（三角面，同样不贴图，用于验证 mesh）----
  void SetHeightMesh(const QImage& heightMap, double width, double depth,
                     double maxHeight);

  void DebugTest(int stage);

 signals:
  void ClickedAt(double x, double y, double z);

 private:
  void setupScene();

  Qt3DRender::QMaterial* createVertexColorMaterial(Qt3DCore::QNode* parent);
  void destroyEntity(Qt3DCore::QEntity*& e);
  void frameCamera(double width, double depth, double maxHeight);
  Qt3DCore::QEntity* pointCloudEntity_ = nullptr;
  Qt3DRender::QParameter* pointSizeParam_ = nullptr;
  float pointSize_ = 3.0f;

  Qt3DExtras::Qt3DWindow* view_ = nullptr;
  QWidget* container_ = nullptr;  // QWindow 嵌入 QWidget 的容器
  Qt3DCore::QEntity* rootEntity_ = nullptr;
  Qt3DCore::QEntity* terrainEntity_ = nullptr;
  Qt3DRender::QCamera* camera_ = nullptr;
  Qt3DExtras::QOrbitCameraController* camController_ = nullptr;

  QLabel* imgLabel_;
};