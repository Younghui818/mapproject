#include "MapMainWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

MapMainWidget::MapMainWidget(QWidget* parent) : QWidget(parent) { InitUI(); }

void MapMainWidget::InitUI() {
  setWindowTitle("MapStudio - Map Demo");

  QHBoxLayout* main_layout = new QHBoxLayout(this);

  // 正常地图显示
  QWidget* map_view_container = new QWidget(this);
  QHBoxLayout* container_layout = new QHBoxLayout(map_view_container);
  container_layout->setContentsMargins(1, 1, 1, 1);
  map_view_ = new MapView(map_view_container);
  container_layout->addWidget(map_view_);
  main_layout->addWidget(map_view_container);

  // 3d地图显示
  w3d_widget_ = new Map3DWidget(this);
  container_layout->addWidget(w3d_widget_);
  w3d_widget_->hide();

  QWidget* widget_operation = new QWidget(this);
  widget_operation->setFixedWidth(240);
  QVBoxLayout* operation_layout = new QVBoxLayout(widget_operation);
  operation_layout->setContentsMargins(1, 1, 1, 1);
  operation_layout->setSpacing(10);
  main_layout->addWidget(widget_operation);

  widget_map_show_ = new QWidget(this);
  widget_map_show_->setObjectName("widget_map_show");
  QVBoxLayout* widget_map_show_layout = new QVBoxLayout(widget_map_show_);
  widget_map_show_layout->setContentsMargins(5, 5, 5, 5);
  widget_map_show_layout->setSpacing(10);
  operation_layout->addWidget(widget_map_show_);

  QWidget* map_sourcer_switcher = new QWidget(widget_map_show_);
  map_sourcer_switcher->setObjectName("map_sourcer_switcher");
  QHBoxLayout* map_sourcer_switcher_layout =
      new QHBoxLayout(map_sourcer_switcher);
  map_sourcer_switcher_layout->setContentsMargins(5, 5, 5, 5);
  btn_god_ = new QRadioButton("AMap", this);
  btn_tianditu_ = new QRadioButton("TianDiTu", this);
  map_sourcer_switcher_layout->addWidget(btn_god_);
  map_sourcer_switcher_layout->addWidget(btn_tianditu_);
  widget_map_show_layout->addWidget(map_sourcer_switcher);

  QWidget* image_switcher = new QWidget(widget_map_show_);
  image_switcher->setObjectName("image_switcher");
  QHBoxLayout* switcher_layout = new QHBoxLayout(image_switcher);
  switcher_layout->setContentsMargins(5, 5, 5, 5);
  btn_rd_ = new QRadioButton("Roads", this);
  btn_st_ = new QRadioButton("Statellite", this);
  switcher_layout->addWidget(btn_rd_);
  switcher_layout->addWidget(btn_st_);
  widget_map_show_layout->addWidget(image_switcher);

  cbx_show_cva_ = new QCheckBox(widget_map_show_);
  cbx_show_cva_->setText("show cva");
  widget_map_show_layout->addWidget(cbx_show_cva_);
  cbx_show_cva_->setChecked(true);

  operation_layout->addSpacing(10);
  btn_enter_3d_ = new QPushButton("enter 3d", widget_operation);
  operation_layout->addWidget(btn_enter_3d_);

  widget_3d_map_show_ = new QWidget(this);
  QVBoxLayout* widget_3d_map_show_layout = new QVBoxLayout(widget_3d_map_show_);

  QWidget* widget_3d_show_type = new QWidget(widget_3d_map_show_);
  QHBoxLayout* widget_3d_show_type_layout =
      new QHBoxLayout(widget_3d_show_type);
  widget_3d_show_type_layout->setContentsMargins(5, 5, 5, 5);
  btn_show_point_ = new QRadioButton("point", this);
  btn_show_mesh_ = new QRadioButton("mesh", this);
  widget_3d_show_type_layout->addWidget(btn_show_point_);
  widget_3d_show_type_layout->addWidget(btn_show_mesh_);
  widget_3d_map_show_layout->addWidget(widget_3d_show_type);

  QWidget* widget_3d_height_scal = new QWidget(widget_3d_map_show_);
  QHBoxLayout* widget_3d_height_scal_layout =
      new QHBoxLayout(widget_3d_height_scal);
  widget_3d_height_scal_layout->setContentsMargins(5, 5, 5, 5);
  slider_3d_height_scal_ =
      new QSlider(Qt::Orientation::Horizontal, widget_3d_map_show_);
  slider_3d_height_scal_->setRange(0, 300);
  slider_3d_height_scal_->setValue(30);
  QLabel* label_3d_height_scal =
      new QLabel("height scale", widget_3d_map_show_);
  widget_3d_height_scal_layout->addWidget(label_3d_height_scal);
  widget_3d_height_scal_layout->addWidget(slider_3d_height_scal_);
  edit_3d_height_scal_ = new QLabel(widget_3d_map_show_);
  edit_3d_height_scal_->setText("3");
  widget_3d_height_scal_layout->addWidget(edit_3d_height_scal_);

  widget_3d_map_show_layout->addWidget(widget_3d_height_scal);

  widget_combo_preset_light_ = new QWidget(widget_3d_map_show_);
  QHBoxLayout* widget_combo_preset_light_layout =
      new QHBoxLayout(widget_combo_preset_light_);
  QLabel* label_combo_preset_light = new QLabel("preset", widget_3d_map_show_);
  widget_combo_preset_light_layout->addWidget(label_combo_preset_light);
  combo_preset_light_ = new QComboBox(widget_3d_map_show_);
  combo_preset_light_->addItem("bright");
  combo_preset_light_->addItem("strong");
  combo_preset_light_->addItem("soft");
  combo_preset_light_->addItem("common");
  widget_combo_preset_light_layout->addWidget(combo_preset_light_);
  widget_3d_map_show_layout->addWidget(widget_combo_preset_light_);

  widget_sliderAmbient_ = new QWidget(widget_3d_map_show_);
  QHBoxLayout* widget_sliderAmbient_layout =
      new QHBoxLayout(widget_sliderAmbient_);
  QLabel* label_sliderAmbient = new QLabel("Ambient", widget_3d_map_show_);
  widget_sliderAmbient_layout->addWidget(label_sliderAmbient);
  sliderAmbient_ =
      new QSlider(Qt::Orientation::Horizontal, widget_3d_map_show_);
  sliderAmbient_->setRange(0, 100);
  sliderAmbient_->setValue(50);
  widget_sliderAmbient_layout->addWidget(sliderAmbient_);
  widget_3d_map_show_layout->addWidget(widget_sliderAmbient_);

  widget_sliderAzimuth_ = new QWidget(widget_3d_map_show_);
  QHBoxLayout* widget_sliderAzimuth_layout =
      new QHBoxLayout(widget_sliderAzimuth_);
  QLabel* label_sliderAzimuth = new QLabel("Azimuth", widget_3d_map_show_);
  widget_sliderAzimuth_layout->addWidget(label_sliderAzimuth);
  sliderAzimuth_ =
      new QSlider(Qt::Orientation::Horizontal, widget_3d_map_show_);
  sliderAzimuth_->setRange(0, 359);
  sliderAzimuth_->setValue(90);
  widget_sliderAzimuth_layout->addWidget(sliderAzimuth_);
  widget_3d_map_show_layout->addWidget(widget_sliderAzimuth_);

  widget_sliderElev_ = new QWidget(widget_3d_map_show_);
  QHBoxLayout* widget_sliderElev_layout = new QHBoxLayout(widget_sliderElev_);
  QLabel* label_sliderElev = new QLabel("Elev", widget_3d_map_show_);
  widget_sliderElev_layout->addWidget(label_sliderElev);
  sliderElev_ = new QSlider(Qt::Orientation::Horizontal, widget_3d_map_show_);
  sliderElev_->setRange(5, 89);
  sliderElev_->setValue(50);
  widget_sliderElev_layout->addWidget(sliderElev_);
  widget_3d_map_show_layout->addWidget(widget_sliderElev_);

  widget_sliderBright_ = new QWidget(widget_3d_map_show_);
  QHBoxLayout* widget_sliderBright_layout =
      new QHBoxLayout(widget_sliderBright_);
  QLabel* label_sliderBright = new QLabel("Bright", widget_3d_map_show_);
  widget_sliderBright_layout->addWidget(label_sliderBright);
  sliderBright_ = new QSlider(Qt::Orientation::Horizontal, widget_3d_map_show_);
  sliderBright_->setRange(20, 300);
  sliderBright_->setValue(100);
  widget_sliderBright_layout->addWidget(sliderBright_);
  widget_3d_map_show_layout->addWidget(widget_sliderBright_);

  operation_layout->addWidget(widget_3d_map_show_);
  widget_3d_map_show_->hide();

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

  connect(btn_enter_3d_, &QPushButton::clicked, this,
          &MapMainWidget::SlotOnEnter3DClicked);
  connect(btn_show_point_, &QRadioButton::clicked, this,
          &MapMainWidget::SlotUpdate3DMapShow);
  connect(btn_show_mesh_, &QRadioButton::clicked, this,
          &MapMainWidget::SlotUpdate3DMapShow);
  connect(slider_3d_height_scal_, &QSlider::valueChanged, this,
          &MapMainWidget::SlotUpdate3dHeightScal);

  // 环境光：0~100 → 0.0~1.0
  connect(sliderAmbient_, &QSlider::valueChanged, this,
          [this](int v) { w3d_widget_->SetAmbient(v / 100.0f); });
  // 方位角：0~359
  connect(sliderAzimuth_, &QSlider::valueChanged, this,
          [this](int v) { w3d_widget_->SetLightAzimuth(float(v)); });
  // 高度角：5~89
  connect(sliderElev_, &QSlider::valueChanged, this,
          [this](int v) { w3d_widget_->SetLightElevation(float(v)); });
  // 亮度：20~300 → 0.2~3.0
  connect(sliderBright_, &QSlider::valueChanged, this,
          [this](int v) { w3d_widget_->SetBrightness(v / 100.0f); });
  connect(combo_preset_light_, &QComboBox::currentIndexChanged, this,
          [this](int index) { w3d_widget_->ApplyLightPreset(index); });
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

void MapMainWidget::SlotOnEnter3DClicked() {
  if (!map_view_) return;

  const bool map_view_show = map_view_->isVisible();
  if (!map_view_show) {
    btn_enter_3d_->setText("enter 3d");
    map_view_->setVisible(true);
    w3d_widget_->setVisible(false);
    widget_map_show_->setVisible(true);
    widget_3d_map_show_->setVisible(false);
    return;
  }

  // 判断亮度调节显示哪些内容
  widget_sliderAmbient_->setVisible(btn_show_point_->isChecked());
  widget_sliderAzimuth_->setVisible(btn_show_point_->isChecked());
  widget_sliderElev_->setVisible(btn_show_point_->isChecked());

  // ---- 1. 中心经纬度 ----
  QRect vp = map_view_->viewport()->rect();
  QPointF centerScene = map_view_->mapToScene(vp.center());
  double lat = 0, lon = 0;
  if (!map_view_->SceneToLatLon(centerScene, lat, lon)) return;

  // ---- 2. 反算 metersPerPixel：取视口中心水平方向 100px 的两点 ----
  double latB = 0, lonB = 0;
  const int probe = 100;
  QPointF probeScene = map_view_->mapToScene(vp.center() + QPoint(probe, 0));
  double metersPerPixel = 30.0;  // 兜底默认值
  if (map_view_->SceneToLatLon(probeScene, latB, lonB)) {
    const double dLon = std::abs(lonB - lon);
    if (dLon > 1e-12) {
      // 经度差 → 东西向距离（乘 cos(lat)）
      const double meters =
          dLon * M_PI / 180.0 * 6371000.0 * std::cos(lat * M_PI / 180.0);
      metersPerPixel = meters / probe;
    }
  }
  qDebug() << "[2d] center" << lat << lon << "mpp" << metersPerPixel;

  // ---- 3. 用真实可视范围建 3D ----
  connect(w3d_widget_, &Map3DWidget::SignalReady, this,
          [this]() { qDebug() << "3D ready"; });
  connect(w3d_widget_, &Map3DWidget::Failed, this,
          [this](const QString& reason) {
            qWarning() << "3D build failed:" << reason;
          });

  w3d_widget_->Build3DFromView(lat, lon, metersPerPixel, vp.width(),
                               vp.height());

  btn_show_point_->setChecked(true);
  btn_enter_3d_->setText("quit 3d");

  map_view_->setVisible(false);
  w3d_widget_->setVisible(true);
  widget_map_show_->setVisible(false);
  widget_3d_map_show_->setVisible(true);
}

void MapMainWidget::SlotUpdate3DMapShow() {
  // 切换3d显示模式
  if (w3d_widget_) {
    if (btn_show_point_->isChecked()) {
      w3d_widget_->UpdateMap3dShowType(kMap3DShowTypePoint);
    } else if (btn_show_mesh_->isChecked()) {
      w3d_widget_->UpdateMap3dShowType(kMap3DShowTypeMesh);
    } else {
      w3d_widget_->UpdateMap3dShowType(kMap3DShowTypeTexture);
    }
  }
  widget_sliderAmbient_->setVisible(btn_show_point_->isChecked());
  widget_sliderAzimuth_->setVisible(btn_show_point_->isChecked());
  widget_sliderElev_->setVisible(btn_show_point_->isChecked());
}

void MapMainWidget::SlotUpdate3dHeightScal(int scal) {
  if (w3d_widget_) {
    w3d_widget_->SetDemExaggeration(scal / 10);
    edit_3d_height_scal_->setText(QString::number(scal / 10.0, 'f', 1));
  }
}