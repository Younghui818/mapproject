#include "MapMainWidget.h"

#include <QVBoxLayout>

MapMainWidget::MapMainWidget(QWidget* parent) : QWidget(parent) { InitUI(); }

void MapMainWidget::InitUI() {
  setWindowTitle("MapStudio - OSM Map Demo");
  resize(1024, 768);

  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  QWidget* image_switcher = new QWidget(this);
  QHBoxLayout* switcher_layout = new QHBoxLayout(image_switcher);
  // 创建按钮
  btn_rd_ = new QRadioButton("Roads", this);
  btn_st_ = new QRadioButton("Streets", this);
  btn_rd_->setFixedSize(100, 30);
  btn_st_->setFixedSize(100, 30);
  switcher_layout->setContentsMargins(0, 0, 0, 0);  // 设置布局的边距为 0
  switcher_layout->addWidget(btn_rd_);
  switcher_layout->addWidget(btn_st_);
  switcher_layout->addStretch(1);      // 添加弹性空间，使按钮靠左对齐
  image_switcher->setFixedHeight(30);  // 设置切换器的高度

  // 创建地图视图容器
  QWidget* mapViewContainer = new QWidget(this);
  QVBoxLayout* containerLayout = new QVBoxLayout(mapViewContainer);
  containerLayout->setContentsMargins(0, 0, 0, 0);  // 去掉边距
  map_view_ = new MapView(mapViewContainer);
  containerLayout->addWidget(map_view_);

  mainLayout->addWidget(image_switcher);
  mainLayout->addWidget(mapViewContainer);

  connect(btn_rd_, &QRadioButton::clicked, this, [this]() {
    map_view_->SwitchTileServer(kImageTypeRoads);
    update();
  });
  connect(btn_st_, &QRadioButton::clicked, this, [this]() {
    map_view_->SwitchTileServer(kImageTypeSt);
    update();
  });

  btn_rd_->setChecked(true);
}