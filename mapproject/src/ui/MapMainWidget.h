#ifndef MAP_MAIN_WIDGET_H_
#define MAP_MAIN_WIDGET_H_

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Map3DWidget.h"
#include "mapview.h"

// 应用主窗口：编排 2D 地图、3D 地形、地图源选择和路径管理控件。
class MapMainWidget : public QWidget {
  Q_OBJECT

 public:
  // 创建主窗口并初始化地图视图和操作控件。
  explicit MapMainWidget(QWidget* parent = nullptr);
  // 控件由 Qt 对象树管理，使用默认析构函数。
  ~MapMainWidget() override = default;

 private slots:
  // 响应路径采集复选框状态变化。
  void SlotStartMouseAddPathChecked(int state);
  // 清除 2D/3D 视图中的路径。
  void SlotClearAllPath();
  // 响应地图源、底图类型或注记开关变化。
  void SlotUpdateMapSourceImage();
  // 结束并保存当前路径。
  void SlotEndPath();
  // 任一地图视图保存路径后更新主窗口路径列表。
  void SlotOnPathSaved(quint64 id, const QString& name, double lengthMeters);
  // 从当前 2D 地图视口进入 3D 地形视图。
  void SlotOnEnter3DClicked();
  // 响应点云/网格显示模式变化。
  void SlotUpdate3DMapShow();
  // 响应 3D 垂直夸张滑块变化；scale 使用 UI 的整数刻度。
  void SlotUpdate3DHeightScale(int scale);
  // 响应路径线宽滑块变化。
  void SlotPathWidthChanged(int width);
  // 打开颜色选择器并更新路径颜色。
  void SlotPickPathColor();
  // 响应“应用到所有路径”复选框变化。
  void SlotApplyAllPathsStyleChanged(int state);

 private:
  // 初始化界面及其四个功能区域。
  // 创建主布局并建立默认控件状态。
  void InitUI();
  // 创建并放置 2D 地图与 3D 地图视图。
  void InitMapShowWidget(QHBoxLayout* main_layout);
  // 创建地图数据源、底图类型和注记控制区域。
  void InitMapSourceDataWidget(QVBoxLayout* operation_layout);
  // 创建 3D 显示模式和光照控制区域。
  void Init3DMapDataWidget(QVBoxLayout* operation_layout);
  // 创建路径采集、列表和样式控制区域。
  void InitMapPathDataWidget(QVBoxLayout* operation_layout);
  // 建立控件、2D MapView 和 3D Map3DWidget 之间的信号槽连接。
  void SignalsLink();

  // 将当前路径宽度和颜色同步到 2D/3D 视图。
  void ApplyPathStyle();         // 统一下发到 2D / 3D
                                 // 根据当前路径颜色刷新颜色按钮的背景样式。
  void UpdatePathColorButton();  // 刷新颜色按钮外观

  // 两个视图共享路径样式和路径业务状态，但同一时刻只显示一个地图视图。
  Map3DWidget* w3d_widget_ = nullptr;  // 3D 地形视图控件，作为主窗口子对象
  MapView* map_view_ = nullptr;        // 2D 瓦片地图视图控件，作为主窗口子对象

  QWidget* widget_map_show_ = nullptr;    // 地图源、底图样式和注记设置容器
  QRadioButton* btn_rd_ = nullptr;        // 标准道路底图选项
  QRadioButton* btn_st_ = nullptr;        // 卫星影像底图选项
  QRadioButton* btn_god_ = nullptr;       // 高德地图源选项
  QRadioButton* btn_tianditu_ = nullptr;  // 天地图源选项
  QCheckBox* cbx_show_cva_ = nullptr;     // 是否显示地图注记层

  QPushButton* btn_enter_3d_ = nullptr;  // 启动 3D 构建并切换到 3D 视图的按钮

  QWidget* widget_3d_map_show_ = nullptr;     // 3D 显示与光照设置容器
  QRadioButton* btn_show_point_ = nullptr;    // 点云显示模式选项
  QRadioButton* btn_show_mesh_ = nullptr;     // 三角网格显示模式选项
  QSlider* slider_3d_height_scal_ = nullptr;  // 垂直夸张倍数滑块
  QLabel* edit_3d_height_scal_ = nullptr;     // 垂直夸张当前数值文本

  QWidget* widget_combo_preset_light_ = nullptr;  // 光照预设控件容器
  QWidget* widget_sliderAmbient_ = nullptr;       // 环境光滑块容器
  QWidget* widget_sliderAzimuth_ = nullptr;       // 方位角滑块容器
  QWidget* widget_sliderElev_ = nullptr;          // 高度角滑块容器
  QWidget* widget_sliderBright_ = nullptr;        // 亮度滑块容器
  QComboBox* combo_preset_light_ = nullptr;       // 光照预设选择框
  QSlider* sliderAmbient_ = nullptr;              // 环境光：0~100 → 0.0~1.0
  QSlider* sliderAzimuth_ = nullptr;              // 方位角：0~359
  QSlider* sliderElev_ = nullptr;                 // 高度角：5~89
  QSlider* sliderBright_ = nullptr;               // 亮度：20~300 → 0.2~3.0

  QCheckBox* cbx_mouse_add_point_ = nullptr;   // 开启/关闭路径点采集
  QPushButton* btn_clear_all_path_ = nullptr;  // 清空所有路径的按钮

  // 结束路径 & 路径列表容器
  QPushButton* btn_end_path_ = nullptr;       // 结束并保存当前路径的按钮
  QWidget* paths_list_widget_ = nullptr;      // 已保存路径列表的容器
  QVBoxLayout* paths_list_layout_ = nullptr;  // 路径列表项的垂直布局

  // 路径样式
  QWidget* widget_path_style_ = nullptr;      // 路径样式设置区域
  QSlider* slider_path_width_ = nullptr;      // 路径线宽滑块，单位为像素
  QLabel* label_path_width_ = nullptr;        // 当前线宽数值显示
  QPushButton* btn_path_color_ = nullptr;     // 打开路径颜色选择器的按钮
  QCheckBox* cbx_apply_all_paths_ = nullptr;  // 是否把样式应用到全部已保存路径
  int path_width_ = 3;                        // 当前共享路径线宽（像素）
  QColor path_color_ = QColor(255, 128, 0);   // 当前共享路径颜色

  // pathId → 列表行索引（用于动画按钮回调）
  QMap<quint64, int> path_row_index_;
  int next_row_index_ = 0;
};

#endif  // MAP_MAIN_WIDGET_H_