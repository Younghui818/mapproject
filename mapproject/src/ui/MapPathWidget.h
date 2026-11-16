#pragma once
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Map3DWidget.h"
#include "MapPathExportDialog.h"
#include "MapView.h"
#include "PathDataModel.h"
#include "RouteService.h"

enum MapViewType { kMapViewType2D = 0, kMapViewType3D = 1 };

class MapPathWidget : public QWidget {
  Q_OBJECT
 public:
  // 初始化路径相关控件。
  explicit MapPathWidget(MapView* w2d_view, Map3DWidget* w3d_widget, QWidget* parent = nullptr);
  // 控件由 Qt 对象树管理，使用默认析构函数。
  ~MapPathWidget() = default;
  void UpdateMapStatus(bool status_3d);

 private slots:
  // 响应路径采集复选框状态变化。
  void SlotStartMouseAddPathChecked(int state);
  // 清除 2D/3D 视图中的路径。
  void SlotClearAllPath();
  // 结束并保存当前路径。
  void SlotEndPath();
  // 响应路径线宽滑块变化。
  void SlotPathWidthChanged(int width);
  // 打开颜色选择器并更新路径颜色。
  void SlotPickPathColor();
  // 响应“应用到所有路径”复选框变化。
  void SlotApplyAllPathsStyleChanged(int state);
  // 导出/导入路径点数据。
  void SlotExportPathClicked();
  void SlotImportPathClicked();

  // 添加当前路径点
  void SlotAddPathPoint();
  // 任一地图视图保存路径后更新主窗口路径列表。
  void SlotOnPathSaved(PathData data);
  void SlotOnRouteReady(const RouteResult& result);

 private:
  void InitUI();
  void SignalLink();

  // 将当前路径宽度和颜色同步到 2D/3D 视图。
  void ApplyPathStyle();         // 统一下发到 2D / 3D
  void UpdatePathColorButton();  // 刷新颜色按钮外观
  void ShowExportDialog();

  QVector<PathExportItem> saved_path_infos_;

  Map3DWidget* w3d_widget_ = nullptr;         // 3D 地形视图控件
  MapView* map_view_ = nullptr;               // 2D 瓦片地图视图控件
  PathDataModel* path_data_model_ = nullptr;  // 路径数据模型
  RouteService* route_service_ = nullptr;     // 路径规划服务

  QCheckBox* cbx_mouse_add_point_ = nullptr;  // 开启/关闭路径点采集
  QCheckBox* cbx_apply_road_ = nullptr;       // 是否应用已知道路

  QPushButton* btn_end_path_ = nullptr;        // 结束并保存当前路径的按钮
  QPushButton* btn_clear_all_path_ = nullptr;  // 清空所有路径的按钮
  QPushButton* btn_export_path_ = nullptr;     // 导出路径按钮
  QPushButton* btn_import_path_ = nullptr;     // 导入路径按钮

  // 结束路径 & 路径列表容器
  QWidget* paths_list_widget_ = nullptr;      // 已保存路径列表的容器
  QVBoxLayout* paths_list_layout_ = nullptr;  // 路径列表项的垂直布局

  // 路径样式
  QSlider* slider_path_width_ = nullptr;      // 路径线宽滑块，单位为像素
  QLabel* label_path_width_ = nullptr;        // 当前线宽数值显示
  QPushButton* btn_path_color_ = nullptr;     // 打开路径颜色选择器的按钮
  QCheckBox* cbx_apply_all_paths_ = nullptr;  // 是否把样式应用到全部已保存路径

  // pathId → 列表行索引（用于动画按钮回调）
  QMap<quint64, int> path_row_index_;
  int next_row_index_ = 0;

  int current_path_width_ = 3;                       // 当前共享路径线宽（像素）
  QColor current_path_color_ = QColor(255, 128, 0);  // 当前共享路径颜色
  MapViewType view_type_ = kMapViewType2D;   // 区分当前是 2D 地图还是 3D 地形视图
  QVector<quint64> saved_path_ids_;          // 与其他已保存路径数组按索引对应的唯一 ID(2d/3d同步)
};
