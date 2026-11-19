#include "AppMainWidget.h"

AppMainWidget::AppMainWidget(QWidget* parent) : QWidget(parent) { InitUI(); }

void AppMainWidget::InitUI() {
  // 设置主布局
  QVBoxLayout* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);

  tab_widget_ = new QTabWidget(this);
  // 初始化地形图和天体窗口
  map_main_widget_ = new MapMainWidget(this);
  tab_widget_->addTab(map_main_widget_, tr("terrain"));
  celestial_main_widget_ = new CelestialMainWidget(this);
  tab_widget_->addTab(celestial_main_widget_, tr("celestial"));

  main_layout->addWidget(tab_widget_);
  // TOOD：
  tab_widget_->setCurrentIndex(1);
}

void AppMainWidget::SingalLink() {
  connect(tab_widget_, &QTabWidget::currentChanged, this, [](int index) { qDebug() << "切换到 Tab:" << index; });
}