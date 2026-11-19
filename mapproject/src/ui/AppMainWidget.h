#ifndef APP_MAIN_WIDGET_H_
#define APP_MAIN_WIDGET_H_

#include <QWidget>
#include <QTabWidget>

#include "MapMainWidget.h"
#include "CelestialMainWidget.h"

class AppMainWidget : public QWidget {
  Q_OBJECT

 public:
  // 创建主窗口
  explicit AppMainWidget(QWidget* parent = nullptr);
  // 控件由 Qt 对象树管理，使用默认析构函数。
  ~AppMainWidget() override = default;

 private:
  void InitUI();
  void SingalLink();

  QTabWidget* tab_widget_ = nullptr;
  MapMainWidget* map_main_widget_ = nullptr;
  CelestialMainWidget* celestial_main_widget_ = nullptr;
};

#endif  // APP_MAIN_WIDGET_H_
