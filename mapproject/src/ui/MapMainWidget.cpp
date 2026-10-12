#include "MapMainWidget.h"

#include <QVBoxLayout>

MapMainWidget::MapMainWidget(QWidget* parent) : QWidget(parent) { InitUI(); }

void MapMainWidget::InitUI() {
  setWindowTitle("MapStudio - OSM Map Demo");
  resize(1024, 768);

  QHBoxLayout* main_layout = new QHBoxLayout(this);

  // 创建地图视图容器
  QWidget* map_view_container = new QWidget(this);
  QHBoxLayout* container_layout = new QHBoxLayout(map_view_container);
  container_layout->setContentsMargins(1, 1, 1, 1);  // 去掉边距
  map_view_ = new MapView(map_view_container);
  container_layout->addWidget(map_view_);
  main_layout->addWidget(map_view_container);
  // 右侧边栏
  QWidget* widget_operation = new QWidget(this);
  widget_operation->setFixedWidth(200);
  QVBoxLayout* operation_layout = new QVBoxLayout(widget_operation);
  operation_layout->setContentsMargins(1, 1, 1, 1);
  operation_layout->setSpacing(10);
  main_layout->addWidget(widget_operation);

  // 地图显示部分
  QWidget* widget_map_show = new QWidget(this);
  widget_map_show->setObjectName("widget_map_show");
  QVBoxLayout* widget_map_show_layout = new QVBoxLayout(widget_map_show);
  widget_map_show_layout->setContentsMargins(5, 5, 5, 5);
  widget_map_show_layout->setSpacing(10);
  operation_layout->addWidget(widget_map_show);

  // 地图源
  QWidget* map_sourcer_switcher = new QWidget(widget_map_show);
  map_sourcer_switcher->setObjectName("map_sourcer_switcher");
  QHBoxLayout* map_sourcer_switcher_layout =
      new QHBoxLayout(map_sourcer_switcher);
  map_sourcer_switcher_layout->setContentsMargins(5, 5, 5, 5);
  btn_god_ = new QRadioButton("AMap", this);
  btn_tianditu_ = new QRadioButton("TianDiTu", this);
  map_sourcer_switcher_layout->addWidget(btn_god_);
  map_sourcer_switcher_layout->addWidget(btn_tianditu_);
  widget_map_show_layout->addWidget(map_sourcer_switcher);

  // 地图模式切换
  QWidget* image_switcher = new QWidget(widget_map_show);
  image_switcher->setObjectName("image_switcher");
  QHBoxLayout* switcher_layout = new QHBoxLayout(image_switcher);
  switcher_layout->setContentsMargins(5, 5, 5, 5);
  btn_rd_ = new QRadioButton("Roads", this);
  btn_st_ = new QRadioButton("Statellite", this);
  switcher_layout->addWidget(btn_rd_);
  switcher_layout->addWidget(btn_st_);
  widget_map_show_layout->addWidget(image_switcher);

  // 是否显示注记信息
  cbx_show_cva_ = new QCheckBox(widget_map_show);
  cbx_show_cva_->setText("show cva");
  widget_map_show_layout->addWidget(cbx_show_cva_);
  cbx_show_cva_->setChecked(true);

  // 创建路线按钮
  operation_layout->addSpacing(50);
  cbx_mouse_add_point_ = new QCheckBox(widget_operation);
  cbx_mouse_add_point_->setText("add path");
  operation_layout->addWidget(cbx_mouse_add_point_);

  // 清除全部路线
  btn_clear_all_path_ = new QPushButton("clear all", widget_operation);
  operation_layout->addWidget(btn_clear_all_path_);
  operation_layout->addStretch(5);

  btn_god_->setChecked(true);
  btn_god_->clicked(true);
  SignalsLink();
  btn_rd_->setChecked(true);
  btn_rd_->clicked(true);
}

void MapMainWidget::SignalsLink() {
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
  connect(cbx_mouse_add_point_, &QCheckBox::stateChanged, this,
          &MapMainWidget::SlotStartMouseAddPathChecked);
  connect(btn_clear_all_path_, &QPushButton::clicked, this,
          &MapMainWidget::SlotClearAllPath);
}

void MapMainWidget::SlotUpdateMapSourceImage() {
  map_view_->SwitchTileServer(
      btn_god_->isChecked() ? kMapSourcesAMap : kMapSourcesTianDiTu,
      btn_rd_->isChecked() ? kImageTypeRoads : kImageTypeSt,
      cbx_show_cva_->isChecked());
  update();
}

void MapMainWidget::SlotStartMouseAddPathChecked(int state) {
  if (state) {
    // 开始鼠标点选路径点
  }
}

void MapMainWidget::SlotClearAllPath() {}