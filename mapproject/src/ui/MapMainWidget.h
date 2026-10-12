#ifndef MAP_MAIN_WIDGET_H_
#define MAP_MAIN_WIDGET_H_

#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>

#include "mapview.h"

class MapMainWidget : public QWidget {
  Q_OBJECT

 public:
  explicit MapMainWidget(QWidget* parent = nullptr);
  ~MapMainWidget() override = default;

 signals:

 private slots:
  void SlotStartMouseAddPathChecked(int state);
  void SlotClearAllPath();
  void SlotUpdateMapSourceImage();

 private:
  void InitUI();
  void SignalsLink();

  QRadioButton* btn_rd_ = nullptr;
  QRadioButton* btn_st_ = nullptr;

  QRadioButton* btn_god_ = nullptr;
  QRadioButton* btn_tianditu_ = nullptr;

  QCheckBox* cbx_show_cva_ = nullptr;
  QCheckBox* cbx_mouse_add_point_ = nullptr;
  QPushButton* btn_clear_all_path_ = nullptr;

  MapView* map_view_ = nullptr;
};

#endif  // MAP_MAIN_WIDGET_H_