#include "MapMainWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

MapMainWidget::MapMainWidget(QWidget* parent) : QWidget(parent) { InitUI(); }

void MapMainWidget::InitUI() {
  setWindowTitle("MapStudio - OSM Map Demo");
  resize(1024, 768);

  QHBoxLayout* main_layout = new QHBoxLayout(this);

  QWidget* map_view_container = new QWidget(this);
  QHBoxLayout* container_layout = new QHBoxLayout(map_view_container);
  container_layout->setContentsMargins(1, 1, 1, 1);
  map_view_ = new MapView(map_view_container);
  container_layout->addWidget(map_view_);
  main_layout->addWidget(map_view_container);

  QWidget* widget_operation = new QWidget(this);
  widget_operation->setFixedWidth(240);
  QVBoxLayout* operation_layout = new QVBoxLayout(widget_operation);
  operation_layout->setContentsMargins(1, 1, 1, 1);
  operation_layout->setSpacing(10);
  main_layout->addWidget(widget_operation);

  QWidget* widget_map_show = new QWidget(this);
  widget_map_show->setObjectName("widget_map_show");
  QVBoxLayout* widget_map_show_layout = new QVBoxLayout(widget_map_show);
  widget_map_show_layout->setContentsMargins(5, 5, 5, 5);
  widget_map_show_layout->setSpacing(10);
  operation_layout->addWidget(widget_map_show);

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

  QWidget* image_switcher = new QWidget(widget_map_show);
  image_switcher->setObjectName("image_switcher");
  QHBoxLayout* switcher_layout = new QHBoxLayout(image_switcher);
  switcher_layout->setContentsMargins(5, 5, 5, 5);
  btn_rd_ = new QRadioButton("Roads", this);
  btn_st_ = new QRadioButton("Statellite", this);
  switcher_layout->addWidget(btn_rd_);
  switcher_layout->addWidget(btn_st_);
  widget_map_show_layout->addWidget(image_switcher);

  cbx_show_cva_ = new QCheckBox(widget_map_show);
  cbx_show_cva_->setText("show cva");
  widget_map_show_layout->addWidget(cbx_show_cva_);
  cbx_show_cva_->setChecked(true);

  operation_layout->addSpacing(10);
  cbx_mouse_add_point_ = new QCheckBox(widget_operation);
  cbx_mouse_add_point_->setText("add path");
  operation_layout->addWidget(cbx_mouse_add_point_);

  // End path button
  btn_end_path_ = new QPushButton("End Path", widget_operation);
  operation_layout->addWidget(btn_end_path_);

  btn_clear_all_path_ = new QPushButton("clear all", widget_operation);
  operation_layout->addWidget(btn_clear_all_path_);

  // 路径列表容器
  paths_list_widget_ = new QWidget(widget_operation);
  paths_list_layout_ = new QVBoxLayout(paths_list_widget_);
  paths_list_layout_->setContentsMargins(2, 2, 2, 2);
  paths_list_layout_->setSpacing(4);
  operation_layout->addWidget(new QLabel("Saved Paths:", widget_operation));
  operation_layout->addWidget(paths_list_widget_);
  operation_layout->addStretch(5);

  btn_god_->setChecked(true);
  SignalsLink();
  btn_rd_->setChecked(true);
  SlotUpdateMapSourceImage();
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

  connect(btn_end_path_, &QPushButton::clicked, this,
          &MapMainWidget::SlotEndPath);

  // 监听 map_view 的 path saved/removed 信号
  connect(map_view_, &MapView::PathSaved, this,
          &MapMainWidget::SlotOnPathSaved);
  connect(map_view_, &MapView::PathRemoved, this,
          &MapMainWidget::SlotOnPathRemoved);
}

void MapMainWidget::SlotUpdateMapSourceImage() {
  map_view_->SwitchTileServer(
      btn_god_->isChecked() ? kMapSourcesAMap : kMapSourcesTianDiTu,
      btn_rd_->isChecked() ? kImageTypeRoads : kImageTypeSt,
      cbx_show_cva_->isChecked());
  update();
}

void MapMainWidget::SlotStartMouseAddPathChecked(int state) {
  bool enable = (state != 0);
  if (map_view_) {
    map_view_->SetAddingPath(enable);
  }
}

void MapMainWidget::SlotClearAllPath() {
  if (map_view_) {
    map_view_->ClearAllPaths();
    // 清空侧边 UI
    QLayoutItem* child;
    while ((child = paths_list_layout_->takeAt(0)) != nullptr) {
      if (child->widget()) {
        child->widget()->deleteLater();
      }
      delete child;
    }
  }
}

void MapMainWidget::SlotEndPath() {
  if (!map_view_) return;
  // 这里用简单的命名策略：Path <id>
  quint64 id = map_view_->SaveCurrentPath(QString());
  Q_UNUSED(id);
  cbx_mouse_add_point_->setChecked(false);
  // PathSaved 信号会通知并添加侧边 UI
}

void MapMainWidget::SlotOnPathSaved(quint64 id, const QString& name) {
  // 新建一行： label + delete button
  QWidget* row = new QWidget(paths_list_widget_);
  QHBoxLayout* hl = new QHBoxLayout(row);
  hl->setContentsMargins(2, 2, 2, 2);
  QLabel* lbl = new QLabel(name, row);
  QPushButton* del = new QPushButton("Delete", row);
  hl->addWidget(lbl);
  hl->addStretch();
  hl->addWidget(del);
  paths_list_layout_->addWidget(row);

  // 删除按钮行为：调用 map_view_->RemoveSavedPathById(id) 并删除 UI 行
  connect(del, &QPushButton::clicked, this, [this, id, row]() {
    if (map_view_) map_view_->RemoveSavedPathById(id);
    // UI 删将在 SlotOnPathRemoved 完成（也可以直接删除）
    row->deleteLater();
  });
}

void MapMainWidget::SlotOnPathRemoved(quint64 id) {
  // 可选：若需要根据 id 找到并移除侧边 UI 行可在此实现（当前 lambda 已直接删除
  // UI 行）
  Q_UNUSED(id);
}