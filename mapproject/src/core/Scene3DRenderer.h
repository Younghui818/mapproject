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

// 3D 地形的显示方式。点云和网格共享同一份高程数据，纹理模式用于贴图显示。
enum Map3DShowType {
  kMap3DShowTypePoint = 0,  // 点云
  kMap3DShowTypeMesh,       // 网格
  kMap3DShowTypeTexture     // 贴图（预留）
};

// 路径点：经纬度 + 存点时的绝对高程（世界 Y）
struct PathPointRecord {
  double lat;  // 创建时的真实地理坐标
  double lon;
  float height;  // 创建时的世界 Y（含夸张）
  double ref_lat;
};

// 使用内存 QImage 提供纹理像素数据的 Qt 3D 纹理图像实现。
class MemoryTextureImage;

// Qt 3D 场景封装：负责地形几何、纹理、光照、相机交互以及路径绘制。
class Scene3DRenderer : public QWidget {
  Q_OBJECT
 public:
  // 创建渲染控件并初始化 Qt 3D 场景。
  explicit Scene3DRenderer(QWidget* parent = nullptr);

  // 设置行优先高程数组；cols/rows 是网格尺寸，width/depth 是平面世界尺寸。
  void SetHeightGrid(const std::vector<float>& heights, int cols, int rows,
                     double width, double depth);  // 真实高程（米）

  // ---- 显示切换和地形外观参数 ----
  // 设置点云、网格或纹理显示模式。
  void SetShowType(Map3DShowType type);
  // 返回当前地形显示模式。
  Map3DShowType ShowType() const { return showType_; }

  // 设置点的屏幕显示尺寸（像素）。
  void SetPointSize(float px);
  // 设置点云采样步长，数值越大显示点越稀疏。
  void SetPointStep(int step);
  // 设置网格构建采样步长，数值越大网格越稀疏。
  void SetMeshStep(int step);
  // 开启或关闭网格线框显示。
  void SetWireframe(bool on);
  // 开启或关闭基于地形法线的阴影着色。
  void SetHillshade(bool on);
  // 设置方向光向量分量，内部会归一化该向量。
  void SetShadeLight(float x, float y, float z);
  // 设置垂直夸张倍数，影响地形高度和路径贴地位置。
  void SetVerticalExaggeration(double k);

  // 返回当前网格中的最低高程（米）。
  double HeightMin() const { return heightMin_; }
  // 返回当前网格中的最高高程（米）。
  double HeightMax() const { return heightMax_; }
  // 清除当前地形、纹理和路径内容。
  void ClearAll();

  // 设置地形纹理图像，并更新对应的 Qt 3D 纹理对象。
  void SetTerrainTexture(const QImage& image);
  // 移除地形纹理并恢复非纹理显示。
  void ClearTerrainTexture();

  // 光照参数。角度只影响颜色计算，不会修改地形几何。
  // 设置环境光强度，输入范围为 0 到 1。
  void SetAmbient(float v);  // 环境光 0~1，越大越亮、越平
  // 设置光源方位角，单位为度。
  void SetLightAzimuth(float deg);  // 方位角 0~360（太阳的罗盘方向）
  // 设置光源高度角，单位为度。
  void SetLightElevation(float deg);  // 高度角 0~90（越高越接近正午）

  // 亮度/对比度只重算颜色缓冲，不改动顶点和索引。
  // 设置颜色亮度系数，1.0 表示不调整。
  void SetBrightness(float v);  // 1.0 = 原始，>1 变亮
  // 设置颜色对比度系数，1.0 表示不调整。
  void SetContrast(float v);  // 1.0 = 原始，>1 对比更强

  // 预设
  // 按预设索引应用一组环境光、方向光和亮度参数。
  void ApplyLightPreset(int idx);  // 0=默认 1=明亮 2=强立体 3=柔和

  // 对单个 RGB 颜色应用当前亮度和对比度设置。
  QVector3D applyBrightnessContrast(const QVector3D& c) const;

  // ---------- 3D 路径绘制 ----------
  // 当前路径和已保存路径内部使用归一化 UV
  // 坐标，地形重建后可重新映射到世界坐标。 开始或结束当前路径的鼠标采集。
  void SetAddingPath(bool enable);
  // 返回当前是否允许用户添加路径点。
  bool IsAddingPath() const { return adding_path_; }
  // 返回当前未保存路径包含的点数。
  int CurrentPathPointCount() const { return current_path_uv_.size(); }

  // 保存当前路径并返回新分配的路径 ID；无有效路径时返回 0。
  quint64 SaveCurrentPath(const QString& name = QString());
  // 按 ID 删除已保存路径。
  void RemoveSavedPathById(quint64 id);
  // 删除当前路径和所有已保存路径。
  void ClearAllPaths();

  // 设置新路径的颜色。
  void SetPathColor(const QColor& c);
  void SetPathWidth(double meters);  // <=0 表示按场景尺度自适应

  // 返回所有已保存路径的当前世界坐标点集合。
  QVector<QVector<QVector3D>> SavedPathsWorld() const;

  // 只影响“正在画 + 之后新建”的路径。
  // 同时设置后续新建路径的像素宽度和颜色。
  void SetPathStyle(int width, const QColor& color);
  // 额外把已保存的所有路径一起改样式。
  // 将像素宽度和颜色应用到当前路径及所有已保存路径。
  void SetAllPathsStyle(int width, const QColor& color);

  // 设置地理坐标原点；lat/lon 为 WGS84 纬度和经度（度）。
  void SetGeoOrigin(double lat, double lon);

  static double TileXToLon(int x, int z);
  static double TileYToLat(int y, int z);
  void SetTileLatLonRange(double lonMin, double lonMax, double latMin,
                          double latMax);

 protected:
  // 过滤嵌入式 Qt 3D 窗口的鼠标与滚轮事件。
  bool eventFilter(QObject* obj, QEvent* ev) override;

 signals:
  // 用户点击地形时发出拾取到的世界坐标（米）。
  void SignalClickedAt(double x, double y, double z);
  // 当前路径新增点后发出当前点数。
  void SignalPathPointAdded(int count);
  // 路径保存完成通知；lengthMeters 为计算得到的长度（米）。
  void SignalPathSaved(quint64 id, const QString& name, double lengthMeters);
  // 已保存路径被删除时发出其唯一 ID。
  void SignalPathRemoved(quint64 id);

  void TerrainChanged();       // 地形几何或地理基准变化时发出
  void PathStyleChanged();     // 路径颜色或线宽变化时发出
  void PathGeometryChanged();  // 当前或已保存路径几何变化时发出

 private:
  // 创建嵌入式 Qt 3D 窗口、根实体、相机和控制器。
  void setupScene();
  // 根据当前数据和显示参数重建地形实体。
  void rebuild();
  // 销毁实体并将调用方保存的指针置空。
  void destroyEntity(Qt3DCore::QEntity*& e);
  // 将相机调整到能够观察当前地形的初始位置。
  void frameCamera();
  // 把当前点尺寸应用到所有点大小节点。
  void applyPointSize();
  // 根据几何对象构造点云/网格实体及所需材质。
  void makeEntity(Qt3DCore::QEntity*& ent, const void* geo, bool asPoints,
                  bool wireframe);
  void updateClipPlanes();          // 把 frameCamera 里的 near/far 计算抽出来
  void doPan(const QPoint& delta);  // 按比例平移

  void refreshColors();  // 只重新计算并覆写顶点颜色缓冲区
  // 根据方位角和高度角生成单位光照方向向量。
  QVector3D lightDirFromAngles() const;
  // 对输入图像按当前亮度和对比度参数进行调整。
  QImage applyImageAdjust(const QImage& src) const;

  // 坐标换算：世界坐标、网格 UV、地理经纬度之间的转换。
  // 将世界坐标映射到归一化网格 UV 坐标。
  QVector2D WorldToGridUV(const QVector3D& w) const;
  // 将网格 UV 坐标转换为包含地形高度的世界坐标。
  QVector3D GridUVToWorld(const QVector2D& uv) const;
  // 在归一化 UV 位置对高程网格做插值采样。
  float HeightAtUV(float u, float v) const;
  // 将世界坐标映射到网格后采样该处地形高度。
  float HeightAtWorld(const QVector3D& w) const;

  // 屏幕点 → 地形交点：先沿射线步进寻找高度符号变化，再用二分细化交点。
  bool PickTerrain(const QPoint& localPos, QVector3D& out, float& outU,
                   float& outV) const;
  // 将裁剪空间坐标逆变换到世界空间。
  static QVector3D Unproject(const QMatrix4x4& invVP, float nx, float ny,
                             float nz);

  // 计算路径线条在世界空间中的半宽。
  float PathHalfWidth() const;
  // 根据世界坐标路径点创建 Qt 3D 路径实体。
  Qt3DCore::QEntity* BuildPathEntity(const QVector<QVector3D>& pts);
  // 使用当前 UV 数据重新生成正在编辑的路径实体。
  void RebuildCurrentPathEntity();
  // 使用保存的 UV 数据重新生成所有已保存路径实体。
  void RefreshAllPathEntities();
  // 将拾取到的世界坐标点追加到当前路径及其地理坐标缓存。
  void AppendPathPoint(const QVector3D& world);
  // UI 像素线宽 → 世界米数的换算基准
  double MetersPerWidthUnit() const;
  // 将 WGS84 坐标转换为相对地理原点的平面世界坐标。
  bool LatLonToWorld(double lat, double lon, QVector3D& out) const;
  QVector3D LatLonToWorldAt(double lat, double lon, double refLat) const;
  void WorldToLatLon(const QVector3D& world, double& lat, double& lon) const;
  // 将地理坐标转换为贴合当前地形表面的世界坐标。
  QVector3D GeoToWorld(double lat, double lon) const;
  // 根据 WGS84 经纬度序列计算路径长度（米）。
  double ComputePathLengthMeters(const QVector<PathPointRecord>& path) const;

  // Qt 3D 窗口及其 QWidget 容器。
  Qt3DExtras::Qt3DWindow* view_ = nullptr;  // 负责实际渲染的 Qt 3D 窗口
  QWidget* container_ = nullptr;  // 将 Qt 3D 窗口嵌入当前 QWidget 的容器
  Qt3DCore::QEntity* rootEntity_ = nullptr;  // 场景实体树的根节点
  Qt3DRender::QCamera* camera_ = nullptr;    // 当前场景使用的相机
  Qt3DExtras::QOrbitCameraController* camController_ =
      nullptr;  // 默认轨道相机控制器

  // 当前地形生成的实体；重建时会销毁并重新创建。
  Qt3DCore::QEntity* pointCloudEntity_ = nullptr;  // 点云显示实体
  Qt3DCore::QEntity* meshEntity_ = nullptr;        // 三角网格显示实体
  QVector<Qt3DRender::QPointSize*>
      pointSizeNodes_;      // 点云材质中的点大小状态节点
  float pointSize_ = 4.0f;  // 点云点尺寸（像素）

  // 真实高程（米），按行优先排列，索引为 row * gridCols_ + col。
  std::vector<float> grid_;                   // 当前地形高程采样数据
  int gridCols_ = 0, gridRows_ = 0;           // 高程网格的列数和行数
  bool hasData_ = false;                      // 是否已经装载有效地形数据
  double heightMin_ = 0.0, heightMax_ = 0.0;  // 当前网格最小/最大高程（米）

  double width_ = 100.0,
         depth_ = 100.0;       // 地形平面范围，分别对应 X/Z 方向（米）
  double exaggeration_ = 1.0;  // 高程垂直夸张倍数

  Map3DShowType showType_ = kMap3DShowTypePoint;  // 当前地形显示模式
  int pointStep_ = 1;                             // 点云抽样步长
  int meshStep_ = 1;                              // 网格抽样步长
  bool wireframe_ = false;                        // 是否显示网格线框
  bool hillshade_ = true;                         // 是否启用地形阴影着色
  QVector3D lightDir_ =
      QVector3D(0.45f, 0.78f, 0.44f).normalized();  // 单位方向光向量
  float ambient_ = 0.35f;                           // 环境光强度，范围为 0 到 1

  // 纹理状态与原始图像；真正的 Qt 3D 纹理对象由 terrainTexImage_ 管理。
  bool useTexture_ = false;  // 是否在网格材质中使用地形纹理
  bool flipV_ = false;       // 是否翻转纹理 V 坐标
  QImage terrainImage_;      // 最近一次设置的原始纹理图像

  bool customZoom_ = true;    // 是否使用本控件自定义滚轮缩放逻辑
  float zoomFactor_ = 0.15f;  // 每个滚轮刻度的缩放比例
  float minDistance_ = 1.0f;  // 相机到观察点允许的最小距离

  bool panning_ = false;    // 是否正在拖动平移场景
  QPoint lastMousePos_;     // 最近一次鼠标位置，用于计算平移增量
  float panFactor_ = 1.2f;  // 平移速度相对于相机距离的系数

  float azimuth_ = 315.0f;   // 光源方位角（度）
  float elevation_ = 45.0f;  // 光源高度角（度）
  float brightness_ = 1.0f;  // 颜色亮度系数，1 表示原始亮度
  float contrast_ = 1.0f;    // 颜色对比度系数，1 表示原始对比度

  MemoryTextureImage* terrainTexImage_ = nullptr;  // Qt 3D 使用的纹理图像对象

  // 状态
  bool adding_path_ = false;      // 是否处于路径采集模式
  QPoint press_pos_;              // 鼠标按下位置，用于区分点击和拖动
  bool potential_click_ = false;  // 当前鼠标按压是否仍可能判定为点击
  QObject* mouse_owner_ =
      nullptr;  // 当前接收鼠标事件的对象，防止容器和窗口重复投递
  const int click_threshold_ = 6;  // 超过该像素距离视为拖拽

  // 当前路径只存归一化 UV，不直接存世界坐标，便于地形缩放或重建后恢复。
  QVector<QVector2D> current_path_uv_;  // 当前路径点的归一化 UV 坐标
  Qt3DCore::QEntity* current_path_entity_ =
      nullptr;  // 当前路径对应的 Qt 3D 实体

  // 已保存路径使用平行数组，所有数组通过相同 index 对齐同一条路径。
  double tile_lon_min_ = 0.0;
  double tile_lon_max_ = 0.0;
  double tile_lat_min_ = 0.0;
  double tile_lat_max_ = 0.0;
  bool tile_range_valid_ = false;

  QVector<quint64> saved_path_ids_;  // 已保存路径的唯一 ID，与其他数组索引对齐
  QVector<QString> saved_path_names_;  // 已保存路径名称，与路径索引对齐
  QVector<Qt3DCore::QEntity*> saved_path_entities_;  // 已保存路径对应的渲染实体

  QVector<QVector<PathPointRecord>> saved_paths_;  // 替代 saved_paths_geo_
  QVector<PathPointRecord> current_path_;          // 替代 current_path_geo_
  QVector<double> saved_path_lengths_m_;           // 已保存路径的长度，单位为米
  bool has_geo_ = false;                           // 是否已设置有效地理原点

  quint64 next_path_id_ = 1;        // 下一个分配给新路径的唯一 ID
  QColor path_color_{230, 60, 60};  // 当前路径颜色
  double path_width_m_ = 0.0;  // 路径世界宽度（米）；0 表示按场景尺寸自适应
  double path_width_ratio_ = 0.006;  // 自适应宽度系数，基于地形最大平面尺寸计算
  double geo_lat_ = 0.0, geo_lon_ = 0.0;  // 地理原点 WGS84 纬度/经度（度）
  int path_width_px_ = 3;  // UI 设定的路径线宽（像素），用于换算世界宽度
};