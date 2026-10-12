#ifndef MAP_MAIN_WIDGET_H_
#define MAP_MAIN_WIDGET_H_

#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

#include "mapview.h"
#include "Map3DWidget.h"

class MapMainWidget : public QWidget {
  Q_OBJECT

 public:
  explicit MapMainWidget(QWidget* parent = nullptr);
  ~MapMainWidget() override = default;

  // UI slot for path saved/removed
 private slots:
  void SlotStartMouseAddPathChecked(int state);
  void SlotClearAllPath();
  void SlotUpdateMapSourceImage();
  void SlotEndPath();
  void SlotOnPathSaved(quint64 id, const QString& name);
  void SlotOnPathRemoved(quint64 id);
  void SlotOnEnter3DClicked();

 private:
  void InitUI();
  void SignalsLink();

  Map3DWidget* w3d_widget_ = nullptr;
  MapView* map_view_ = nullptr;

  QRadioButton* btn_rd_ = nullptr;
  QRadioButton* btn_st_ = nullptr;

  QRadioButton* btn_god_ = nullptr;
  QRadioButton* btn_tianditu_ = nullptr;

  QCheckBox* cbx_show_cva_ = nullptr;

  QPushButton* btn_enter_3d_ = nullptr;

  QCheckBox* cbx_mouse_add_point_ = nullptr;
  QPushButton* btn_clear_all_path_ = nullptr;

  // 新增：结束路径 & 路径列表容器
  QPushButton* btn_end_path_ = nullptr;
  QWidget* paths_list_widget_ = nullptr;
  QVBoxLayout* paths_list_layout_ = nullptr;
};

#endif  // MAP_MAIN_WIDGET_H_