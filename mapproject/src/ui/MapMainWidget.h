#ifndef MAP_MAIN_WIDGET_H_
#define MAP_MAIN_WIDGET_H_

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Map3DOperationWidget.h"
#include "Map3DWidget.h"
#include "MapPathWidget.h"
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
  // 响应地图源、底图类型或注记开关变化。
  void SlotUpdateMapSourceImage();
  // 从当前 2D 地图视口进入 3D 地形视图。
  void SlotOnEnter3DClicked();

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

  // 两个视图共享路径样式和路径业务状态，但同一时刻只显示一个地图视图。
  Map3DWidget* w3d_widget_ = nullptr;  // 3D 地形视图控件，作为主窗口子对象
  MapView* map_view_ = nullptr;        // 2D 瓦片地图视图控件，作为主窗口子对象

  QWidget* widget_map_show_ = nullptr;    // 地图源、底图样式和注记设置容器
  QRadioButton* btn_rd_ = nullptr;        // 标准道路底图选项
  QRadioButton* btn_st_ = nullptr;        // 卫星影像底图选项
  QRadioButton* btn_god_ = nullptr;       // 高德地图源选项
  QRadioButton* btn_tianditu_ = nullptr;  // 天地图源选项
  QCheckBox* cbx_show_cva_ = nullptr;     // 是否显示2D地图注记层

  QPushButton* btn_enter_3d_ = nullptr;  // 启动 3D 构建并切换到 3D 视图的按钮
  Map3DOperationWidget* w3d_operation_widget_ =
      nullptr;  // 3D 地形操作控件，作为主窗口子对象

  MapPathWidget* path_widget_ = nullptr;  // 路径操作控件，作为主窗口子对象
};

#endif  // MAP_MAIN_WIDGET_H_