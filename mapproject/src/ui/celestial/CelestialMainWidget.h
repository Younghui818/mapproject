#ifndef CELESTIAL_MAIN_WIDGET_H_
#define CELESTIAL_MAIN_WIDGET_H_

#include <QWidget>
#include <QPushButton>

#include "CelestialRenderWidget.h"
#include "CelestialOperationWidget.h"

class CelestialMainWidget : public QWidget {
  Q_OBJECT

 public:
  // 创建主窗口
  explicit CelestialMainWidget(QWidget* parent = nullptr);
  // 控件由 Qt 对象树管理，使用默认析构函数。
  ~CelestialMainWidget() override = default;

 private:
  void InitUI();
  void SingalLink();

  CelestialRenderWidget* widget_render_ = nullptr;
  CelestialOperationWidget* widget_operation_ = nullptr;
};

#endif  // CELESTIAL_MAIN_WIDGET_H_