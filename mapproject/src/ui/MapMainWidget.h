#ifndef MAP_MAIN_WIDGET_H_
#define MAP_MAIN_WIDGET_H_

#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Map3DWidget.h"
#include "mapview.h"

class MapMainWidget : public QWidget {
  Q_OBJECT

 public:
  explicit MapMainWidget(QWidget* parent = nullptr);
  ~MapMainWidget() override = default;

 private slots:
  void SlotStartMouseAddPathChecked(int state);
  void SlotClearAllPath();
  void SlotUpdateMapSourceImage();
  void SlotEndPath();
  void SlotOnPathSaved(quint64 id, const QString& name);
  void SlotOnPathRemoved(quint64 id);
  void SlotOnEnter3DClicked();
  void SlotUpdate3DMapShow();
  void SlotUpdate3dHeightScal(int scal);

 private:
  void InitUI();
  void SignalsLink();

  Map3DWidget* w3d_widget_ = nullptr;
  MapView* map_view_ = nullptr;

  QWidget* widget_map_show_ = nullptr;    // 地图源、样式显示
  QRadioButton* btn_rd_ = nullptr;        // 地图样式 标准
  QRadioButton* btn_st_ = nullptr;        // 地图样式 卫星
  QRadioButton* btn_god_ = nullptr;       // 地图源 高德
  QRadioButton* btn_tianditu_ = nullptr;  // 地图源 天地图
  QCheckBox* cbx_show_cva_ = nullptr;     // 地图是否显示注记

  QPushButton* btn_enter_3d_ = nullptr;  // 进入3的地图

  QWidget* widget_3d_map_show_ = nullptr;     // 3d模式下地图样式显示窗口
  QRadioButton* btn_show_point_ = nullptr;    // 点云显示地形
  QRadioButton* btn_show_mesh_ = nullptr;     // 网格显示地形
  QRadioButton* btn_show_texture_ = nullptr;  // 贴图显示地形
  QSlider* slider_3d_height_scal_ = nullptr;  // 3d显示高度对比因子

  QCheckBox* cbx_mouse_add_point_ = nullptr;   // 开始创建路径
  QPushButton* btn_clear_all_path_ = nullptr;  // 清除所有路径

  // 新增：结束路径 & 路径列表容器
  QPushButton* btn_end_path_ = nullptr;   // 结束创建路径
  QWidget* paths_list_widget_ = nullptr;  // 路径显示窗口
  QVBoxLayout* paths_list_layout_ = nullptr;
};

#endif  // MAP_MAIN_WIDGET_H_