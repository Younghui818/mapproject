#ifndef MAP_MAIN_WIDGET_H_
#define MAP_MAIN_WIDGET_H_

#include <QPushButton>
#include <QWidget>
#include <QRadioButton>

#include "mapview.h"

class MapMainWidget : public QWidget {
  Q_OBJECT

 public:
  explicit MapMainWidget(QWidget* parent = nullptr);
  ~MapMainWidget() override = default;

 private:
  void InitUI();

  QRadioButton* btn_rd_ = nullptr;
  QRadioButton* btn_st_ = nullptr;

  MapView* map_view_ = nullptr;
};

#endif  // MAP_MAIN_WIDGET_H_