#pragma once

#include <QEvent>
#include <QImage>
#include <QPoint>
#include <QVector2D>
#include <QVector3D>
#include <QVector>
#include <QWidget>
#include <Qt3DCore/QEntity>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QTextureMaterial>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QPointSize>
#include <Qt3DRender/QTexture>
#include <vector>

#include "MeshBuilder.h"

enum Map3DShowType {
  kMap3DShowTypePoint = 0,  // 点云
  kMap3DShowTypeMesh,       // 网格
  kMap3DShowTypeTexture     // 贴图（预留）
};

class MemoryTextureImage;

class Scene3DRenderer : public QWidget {
  Q_OBJECT
 public:
  explicit Scene3DRenderer(QWidget* parent = nullptr);

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
  void SetVerticalExaggeration(double k);

  double HeightMin() const { return heightMin_; }
  double HeightMax() const { return heightMax_; }
  void ClearAll();

  void SetTerrainTexture(const QImage& image);
  void ClearTerrainTexture();
  void SetTextureFlipV(bool flip);  // 纹理上下颠倒时切换

  // 光照
  void SetAmbient(float v);           // 环境光 0~1，越大越亮、越平
  void SetLightAzimuth(float deg);    // 方位角 0~360（太阳的罗盘方向）
  void SetLightElevation(float deg);  // 高度角 0~90（越高越接近正午）

  // 亮度/对比度（不改几何，只改颜色）
  void SetBrightness(float v);  // 1.0 = 原始，>1 变亮
  void SetContrast(float v);    // 1.0 = 原始，>1 对比更强

  // 预设
  void ApplyLightPreset(int idx);  // 0=默认 1=明亮 2=强立体 3=柔和

  QVector3D applyBrightnessContrast(const QVector3D& c) const;

  // ---------- 3D 路径绘制 ----------
  void SetAddingPath(bool enable);
  bool IsAddingPath() const { return adding_path_; }
  int CurrentPathPointCount() const { return current_path_uv_.size(); }

  quint64 SaveCurrentPath(const QString& name = QString());
  void RemoveSavedPathById(quint64 id);
  void ClearAllPaths();

  void SetPathColor(const QColor& c);
  void SetPathWidth(double meters);  // <=0 表示按场景尺度自适应

  // 与 2D 视图互通（可选）
  void SetGeoOrigin(double lat, double lon) {
    geo_lat_ = lat;
    geo_lon_ = lon;
  }
  bool WorldToLatLon(const QVector3D& w, double& outLat, double& outLon) const;
  QVector<QVector<QVector3D>> SavedPathsWorld() const;

  // 只影响"正在画 + 之后新建"的路径
  void SetPathStyle(int width, const QColor& color);
  // 额外把已保存的所有路径一起改样式
  void SetAllPathsStyle(int width, const QColor& color);

 protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;

 signals:
  void SignalClickedAt(double x, double y, double z);
  void SignalPathPointAdded(int count);
  void SignalPathSaved(quint64 id, const QString& name, double lengthMeters);
  void SignalPathRemoved(quint64 id);

 private:
  void setupScene();
  void rebuild();
  void destroyEntity(Qt3DCore::QEntity*& e);
  void frameCamera();
  void applyPointSize();
  void makeEntity(Qt3DCore::QEntity*& ent, const void* geo, bool asPoints,
                  bool wireframe);
  void updateClipPlanes();  // 把 frameCamera 里的 near/far 计算抽出来
  void SetZoomSensitivity(float f) { zoomFactor_ = f; }
  void doPan(const QPoint& delta);  // 按比例平移

  void refreshColors();  // ★ 只重算并覆写颜色 buffer
  QVector3D lightDirFromAngles() const;
  QImage applyImageAdjust(const QImage& src) const;

  // 坐标换算
  QVector2D WorldToGridUV(const QVector3D& w) const;
  QVector3D GridUVToWorld(const QVector2D& uv) const;
  float HeightAtUV(float u, float v) const;
  float HeightAtWorld(const QVector3D& w) const;

  // 屏幕点 → 地形交点（射线步进 + 二分细化）
  bool PickTerrain(const QPoint& localPos, QVector3D& out) const;
  static QVector3D Unproject(const QMatrix4x4& invVP, float nx, float ny,
                             float nz);

  float PathHalfWidth() const;
  Qt3DCore::QEntity* BuildPathEntity(const QVector<QVector3D>& pts);
  void RebuildCurrentPathEntity();
  void RefreshAllPathEntities();
  void AppendPathPoint(const QVector3D& world);
  // UI 像素线宽 → 世界米数的换算基准
  double MetersPerWidthUnit() const;
  double ComputePathLengthMeters(const QVector<QVector2D>& uv) const;

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

  bool useTexture_ = false;
  bool flipV_ = false;
  QImage terrainImage_;

  bool customZoom_ = true;
  float zoomFactor_ = 0.15f;  // 每格滚轮缩放 15%
  float minDistance_ = 1.0f;

  bool panning_ = false;
  QPoint lastMousePos_;
  float panFactor_ = 1.2f;  // 拖过一屏高度 ≈ 移动 1.2 倍相机距离

  float azimuth_ = 315.0f;  // 默认西北方向打光（制图标西准）
  float elevation_ = 45.0f;
  float brightness_ = 1.0f;
  float contrast_ = 1.0f;

  MemoryTextureImage* terrainTexImage_ = nullptr;

  // 状态
  bool adding_path_ = false;
  QPoint press_pos_;
  bool potential_click_ = false;
  QObject* mouse_owner_ = nullptr;  // 防 container_/view_ 双投递
  const int click_threshold_ = 6;   // 超过 6px 视为拖拽而非点击

  // 当前路径（存归一化 uv，不存世界坐标）
  QVector<QVector2D> current_path_uv_;
  Qt3DCore::QEntity* current_path_entity_ = nullptr;

  // 已保存路径（平行数组，按 index 对齐）
  QVector<QVector<QVector2D>> saved_paths_uv_;
  QVector<quint64> saved_path_ids_;
  QVector<QString> saved_path_names_;
  QVector<Qt3DCore::QEntity*> saved_path_entities_;
  QVector<double> saved_path_lengths_m_;

  quint64 next_path_id_ = 1;
  QColor path_color_{230, 60, 60};
  double path_width_m_ = 0.0;        // 0 → 自适应
  double path_width_ratio_ = 0.006;  // 自适应: max(width_,depth_) * ratio * 0.5
  double geo_lat_ = 0.0, geo_lon_ = 0.0;

  int path_width_px_ = 3;
};