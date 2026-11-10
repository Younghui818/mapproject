#pragma once

#include <QComboBox>
#include <QRadioButton>
#include <QSlider>
#include <QWidget>
#include <QLabel>
#include <QCheckBox>

#include "Map3DWidget.h"

class Map3DOperationWidget : public QWidget {
  Q_OBJECT
 public:
  // 初始化3d地图操作控件。
  explicit Map3DOperationWidget(Map3DWidget* w3d_widget,
                                QWidget* parent = nullptr);

  // 控件由 Qt 对象树管理，使用默认析构函数。
  ~Map3DOperationWidget() = default;

 protected:
  void showEvent(QShowEvent* event) override;

 private slots:
  // 响应点云/网格显示模式变化。
  void SlotUpdate3DMapShow();
  // 响应 3D 垂直夸张滑块变化；scale 使用 UI 的整数刻度。
  void SlotUpdate3DHeightScale(int scale);
  // 是否显示注记层
  void SlotUpdateMapImageShow(bool show);

 private:
  void InitUI();
  void SignalLink();

  Map3DWidget* w3d_widget_ = nullptr;  // 3D 地形视图控件，作为主窗口子对象

  QRadioButton* btn_show_point_ = nullptr;    // 点云显示模式选项
  QRadioButton* btn_show_mesh_ = nullptr;     // 三角网格显示模式选项
  QCheckBox* cbx_show_cva_ = nullptr;         // 是否显示地图注记层
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
};
