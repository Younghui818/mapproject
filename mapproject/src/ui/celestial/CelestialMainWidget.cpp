#include "CelestialMainWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

CelestialMainWidget::CelestialMainWidget(QWidget* parent) : QWidget(parent) {
  InitUI();
  SingalLink();
}

void CelestialMainWidget::InitUI() {
  this->setStyleSheet("background-color: black;");  // 设置背景颜色为黑色
  QHBoxLayout* main_layout = new QHBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);

  widget_render_ = new CelestialRenderWidget(this);
  main_layout->addWidget(widget_render_);

  widget_operation_ = new CelestialOperationWidget(this);
  main_layout->addWidget(widget_operation_);
}

void CelestialMainWidget::SingalLink() {
  connect(widget_operation_, &CelestialOperationWidget::SignalPlanetVisible, widget_render_,
          &CelestialRenderWidget::SlotPlanetVisible);
  connect(widget_operation_, &CelestialOperationWidget::SignalStartSpinAnimation, widget_render_,
          &CelestialRenderWidget::SlotStartSpinAnimation);
  connect(widget_operation_, &CelestialOperationWidget::SignalStartOrbitAnimation, widget_render_,
          &CelestialRenderWidget::SlotStartOrbitAnimation);
}