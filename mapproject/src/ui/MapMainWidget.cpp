#include "MapMainWidget.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

MapMainWidget::MapMainWidget(QWidget* parent) : QWidget(parent) { InitUI(); }

void MapMainWidget::InitUI() {
  // 先创建左右布局，再创建地图源、3D 参数和路径管理三个控制区域。
  setWindowTitle("MapStudio - Map Demo");
  setObjectName("MapMainWidget");

  // 左侧地图显示内容
  QHBoxLayout* main_layout = new QHBoxLayout(this);
  InitMapShowWidget(main_layout);

  // 右侧操作窗口
  QWidget* widget_operation = new QWidget(this);
  widget_operation->setFixedWidth(240);
  QVBoxLayout* operation_layout = new QVBoxLayout(widget_operation);
  operation_layout->setContentsMargins(1, 1, 1, 1);
  operation_layout->setSpacing(10);
  main_layout->addWidget(widget_operation);
  // 地图来源、样式内容
  InitMapSourceDataWidget(operation_layout);

  // 进入、退出3d模式按钮
  operation_layout->addSpacing(10);
  btn_enter_3d_ = new QPushButton("enter 3d", widget_operation);
  btn_enter_3d_->setObjectName("btnPrimary");
  operation_layout->addWidget(btn_enter_3d_);

  // 3d地图相关控件
  Init3DMapDataWidget(operation_layout);

  // 路径设置相关内容
  InitMapPathDataWidget(operation_layout);

  // 进入页面默认设置
  btn_tianditu_->setChecked(true);
  btn_st_->setChecked(true);
  cbx_show_cva_->setChecked(false);
  SignalsLink();
  SlotUpdateMapSourceImage();
}

void MapMainWidget::InitMapShowWidget(
    QHBoxLayout* main_layout) {  // 正常地图显示
  // 2D 和 3D 视图放在同一容器中，通过 show/hide 切换显示状态。
  QWidget* map_view_container = new QWidget(this);
  QHBoxLayout* container_layout = new QHBoxLayout(map_view_container);
  container_layout->setContentsMargins(1, 1, 1, 1);
  map_view_ = new MapView(map_view_container);
  container_layout->addWidget(map_view_);
  main_layout->addWidget(map_view_container);
  // 3d地图显示
  w3d_widget_ = new Map3DWidget(this);
  container_layout->addWidget(w3d_widget_);
  w3d_widget_->hide();
}

void MapMainWidget::InitMapSourceDataWidget(QVBoxLayout* operation_layout) {
  // 创建地图源、底图类型和注记开关。
  widget_map_show_ = new QWidget(this);
  widget_map_show_->setObjectName("widget_map_show");
  QVBoxLayout* widget_map_show_layout = new QVBoxLayout(widget_map_show_);
  widget_map_show_layout->setContentsMargins(5, 5, 5, 5);
  widget_map_show_layout->setSpacing(10);

  QWidget* map_sourcer_switcher = new QWidget(widget_map_show_);
  map_sourcer_switcher->setObjectName("map_sourcer_switcher");
  QHBoxLayout* map_sourcer_switcher_layout =
      new QHBoxLayout(map_sourcer_switcher);
  map_sourcer_switcher_layout->setContentsMargins(5, 5, 5, 5);
  btn_god_ = new QRadioButton("AMap", this);
  btn_tianditu_ = new QRadioButton("TianDiTu", this);
  map_sourcer_switcher_layout->addWidget(btn_god_);
  map_sourcer_switcher_layout->addWidget(btn_tianditu_);
  widget_map_show_layout->addWidget(map_sourcer_switcher);

  QWidget* image_switcher = new QWidget(widget_map_show_);
  image_switcher->setObjectName("image_switcher");
  QHBoxLayout* switcher_layout = new QHBoxLayout(image_switcher);
  switcher_layout->setContentsMargins(5, 5, 5, 5);
  btn_rd_ = new QRadioButton("Roads", this);
  btn_st_ = new QRadioButton("Statellite", this);
  switcher_layout->addWidget(btn_rd_);
  switcher_layout->addWidget(btn_st_);
  widget_map_show_layout->addWidget(image_switcher);

  cbx_show_cva_ = new QCheckBox(widget_map_show_);
  cbx_show_cva_->setText("show cva");
  widget_map_show_layout->addWidget(cbx_show_cva_);
  cbx_show_cva_->setChecked(true);
  operation_layout->addWidget(widget_map_show_);
}

void MapMainWidget::Init3DMapDataWidget(QVBoxLayout* operation_layout) {
  w3d_operation_widget_ = new Map3DOperationWidget(w3d_widget_, this);
  operation_layout->addWidget(w3d_operation_widget_);
}

void MapMainWidget::InitMapPathDataWidget(QVBoxLayout* operation_layout) {
  // 创建路径绘制开关、路径列表以及线宽/颜色设置控件。
  operation_layout->addSpacing(10);
  operation_layout->addWidget(new QLabel("Saved Paths:", this));
  path_widget_ = new MapPathWidget(map_view_, w3d_widget_, this);
  operation_layout->addWidget(path_widget_);
  operation_layout->addStretch(5);
}

void MapMainWidget::SignalsLink() {
  // 使用 Qt 的类型安全连接，将 2D/3D 视图事件统一转发到主窗口槽函数。
  connect(btn_rd_, &QRadioButton::clicked, this,
          &MapMainWidget::SlotUpdateMapSourceImage);
  connect(btn_st_, &QRadioButton::clicked, this,
          &MapMainWidget::SlotUpdateMapSourceImage);
  connect(btn_god_, &QRadioButton::clicked, this,
          &MapMainWidget::SlotUpdateMapSourceImage);
  connect(btn_tianditu_, &QRadioButton::clicked, this,
          &MapMainWidget::SlotUpdateMapSourceImage);
  connect(cbx_show_cva_, &QCheckBox::stateChanged, this,
          &MapMainWidget::SlotUpdateMapSourceImage);

  connect(btn_enter_3d_, &QPushButton::clicked, this,
          &MapMainWidget::SlotOnEnter3DClicked);
}

void MapMainWidget::SlotUpdateMapSourceImage() {
  // 根据单选按钮状态重新配置 2D 地图图层并触发可见瓦片加载。
  map_view_->SwitchTileServer(
      btn_god_->isChecked() ? kMapSourcesAMap : kMapSourcesTianDiTu,
      btn_rd_->isChecked() ? kImageTypeRoads : kImageTypeSt,
      cbx_show_cva_->isChecked());
  update();
}

void MapMainWidget::SlotOnEnter3DClicked() {
  // 从当前 2D 视口读取中心位置和分辨率，启动对应范围的 3D 地形构建。
  if (!map_view_) return;

  const bool map_view_show = map_view_->isVisible();
  if (!map_view_show) {
    btn_enter_3d_->setText("enter 3d");
    map_view_->setVisible(true);
    w3d_widget_->setVisible(false);
    widget_map_show_->setVisible(true);
    w3d_operation_widget_->setVisible(false);
    path_widget_->UpdateMapStatus(false);
    return;
  }

  // ---- 1. 中心经纬度 ----
  QRect vp = map_view_->viewport()->rect();
  QPointF centerScene = map_view_->mapToScene(vp.center());
  double lat = 0, lon = 0;
  if (!map_view_->SceneToLatLon(centerScene, lat, lon)) return;

  // ---- 2. 反算 metersPerPixel：取视口中心水平方向 100px 的两点 ----
  double latB = 0, lonB = 0;
  const int probe = 100;
  QPointF probeScene = map_view_->mapToScene(vp.center() + QPoint(probe, 0));
  double metersPerPixel = 30.0;  // 兜底默认值
  if (map_view_->SceneToLatLon(probeScene, latB, lonB)) {
    const double dLon = std::abs(lonB - lon);
    if (dLon > 1e-12) {
      // 经度差 → 东西向距离（乘 cos(lat)）
      const double meters =
          dLon * M_PI / 180.0 * 6371000.0 * std::cos(lat * M_PI / 180.0);
      metersPerPixel = meters / probe;
    }
  }
  qDebug() << "[2d] center" << lat << lon << "mpp" << metersPerPixel;
  w3d_widget_->Build3DFromView(lat, lon, metersPerPixel, vp.width(),
                               vp.height());
  btn_enter_3d_->setText("quit 3d");
  path_widget_->UpdateMapStatus(true);

  map_view_->setVisible(false);
  w3d_widget_->setVisible(true);
  widget_map_show_->setVisible(false);
  w3d_operation_widget_->setVisible(true);
}