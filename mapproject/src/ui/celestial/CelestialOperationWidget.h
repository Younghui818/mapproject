#ifndef CELESTIAL_OPERATION_WIDGET_H_
#define CELESTIAL_OPERATION_WIDGET_H_

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QVector>

class CelestialOperationWidget : public QWidget {
  Q_OBJECT

 public:
  // 创建主窗口
  explicit CelestialOperationWidget(QWidget* parent = nullptr);
  // 控件由 Qt 对象树管理，使用默认析构函数。
  ~CelestialOperationWidget() override = default;

 private slots:
  void SlotBtnStartClicked();

 signals:
  void SignalPlanetVisible(int planet_index, bool visible);
  void SignalStartSpinAnimation();
  void SignalStartOrbitAnimation();

 private:
  void InitUI();
  void SingalLink();
  QWidget* InitPlanetVisibleWidget();

  QPushButton* btn_spin_start_ = nullptr;
  QPushButton* btn_orbit_start_ = nullptr;
  QCheckBox* cb_sun_ = nullptr;
  QCheckBox* cb_all_planets_ = nullptr;
  QVector<QCheckBox*> cb_planets_;
};

#endif  // CELESTIAL_MAIN_WIDGET_H_