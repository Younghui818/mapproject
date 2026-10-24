#include "MapMainWidget.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
QString FormatLength(double meters) {
  if (meters < 1.0) return QString::number(meters * 100.0, 'f', 1) + " cm";
  if (meters < 1000.0) return QString::number(meters, 'f', 1) + " m";
  return QString::number(meters / 1000.0, 'f', 2) + " km";
}
}  // namespace

MapMainWidget::MapMainWidget(QWidget* parent) : QWidget(parent) { InitUI(); }

void MapMainWidget::InitUI() {
  setWindowTitle("MapStudio - Map Demo");
  setObjectName("MapMainWidget");

  // 左侧地图显示内容
  QHBoxLayout* main_layout = new QHBoxLayout(this);
  InitMapShowWidget(main_layout);

  // 右侧操作窗口
  QWidget* widget_operation = new QWidget(this);
  widget_operation->setFixedWidth(240);
  QVBoxLayout* operation_layout = new QVBoxLayout(widget_operation);
  operation_layout->setContentsMargins(1, 1, 1, 1);
  operation_layout->setSpacing(10);
  main_layout->addWidget(widget_operation);
  // 地图来源、样式内容
  InitMapSourceDataWidget(operation_layout);

  // 进入、退出3d模式按钮
  operation_layout->addSpacing(10);
  btn_enter_3d_ = new QPushButton("enter 3d", widget_operation);
  btn_enter_3d_->setObjectName("btnPrimary");
  operation_layout->addWidget(btn_enter_3d_);

  // 3d地图相关控件
  Init3DMapDataWidget(operation_layout);

  // 路径设置相关内容
  InitMapPathDataWidget(operation_layout);

  // 进入页面默认设置
  btn_god_->setChecked(true);
  btn_rd_->setChecked(true);
  SignalsLink();
  SlotUpdateMapSourceImage();
  UpdatePathColorButton();
  ApplyPathStyle();
}

void MapMainWidget::InitMapShowWidget(
    QHBoxLayout* main_layout) {  // 正常地图显示
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
}

void MapMainWidget::InitMapSourceDataWidget(QVBoxLayout* operation_layout) {
  widget_map_show_ = new QWidget(this);
  widget_map_show_->setObjectName("widget_map_show");
  QVBoxLayout* widget_map_show_layout = new QVBoxLayout(widget_map_show_);
  widget_map_show_layout->setContentsMargins(5, 5, 5, 5);
  widget_map_show_layout->setSpacing(10);

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
  operation_layout->addWidget(widget_map_show_);
}

void MapMainWidget::Init3DMapDataWidget(QVBoxLayout* operation_layout) {
  widget_3d_map_show_ = new QWidget(this);
  widget_3d_map_show_->setObjectName("widget_3d_map_show");
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
}

void MapMainWidget::InitMapPathDataWidget(QVBoxLayout* operation_layout) {
  operation_layout->addSpacing(10);
  widget_path_style_ = new QWidget(this);
  widget_path_style_->setObjectName("widget_path_style");
  QVBoxLayout* widget_path_style_layout = new QVBoxLayout(widget_path_style_);
  widget_path_style_layout->setContentsMargins(5, 5, 5, 5);
  widget_path_style_layout->setSpacing(6);
  // 线宽
  QWidget* w_line_width = new QWidget(widget_path_style_);
  QHBoxLayout* h_line_width = new QHBoxLayout(w_line_width);
  h_line_width->setContentsMargins(0, 0, 0, 0);
  h_line_width->setSpacing(6);
  QLabel* label_line_width = new QLabel("width", w_line_width);
  slider_path_width_ = new QSlider(Qt::Orientation::Horizontal, w_line_width);
  slider_path_width_->setRange(1, 20);
  slider_path_width_->setSingleStep(1);
  slider_path_width_->setPageStep(1);
  slider_path_width_->setValue(path_width_);
  slider_path_width_->setMinimumWidth(80);
  slider_path_width_->setToolTip("path line width (px)");
  label_path_width_ = new QLabel(w_line_width);
  label_path_width_->setFixedWidth(38);
  label_path_width_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  label_path_width_->setText(QString::number(path_width_));
  h_line_width->addWidget(label_line_width);
  h_line_width->addWidget(slider_path_width_, 1);
  h_line_width->addWidget(label_path_width_);
  widget_path_style_layout->addWidget(w_line_width);
  // 颜色
  QWidget* w_line_color = new QWidget(widget_path_style_);
  QHBoxLayout* h_line_color = new QHBoxLayout(w_line_color);
  h_line_color->setContentsMargins(0, 0, 0, 0);
  h_line_color->setSpacing(6);
  QLabel* label_line_color = new QLabel("color", w_line_color);
  btn_path_color_ = new QPushButton(w_line_color);
  btn_path_color_->setObjectName("btnPathColor");
  btn_path_color_->setFixedSize(64, 20);
  btn_path_color_->setCursor(Qt::PointingHandCursor);
  btn_path_color_->setToolTip("click to pick path color");
  h_line_color->addWidget(label_line_color);
  h_line_color->addStretch(1);
  h_line_color->addWidget(btn_path_color_);
  widget_path_style_layout->addWidget(w_line_color);
  // 是否同步应用到已保存路径
  cbx_apply_all_paths_ = new QCheckBox("apply to saved", widget_path_style_);
  cbx_apply_all_paths_->setToolTip(
      "also restyle the paths that are already saved");
  widget_path_style_layout->addWidget(cbx_apply_all_paths_);

  operation_layout->addWidget(widget_path_style_);

  // 添加管理路径相关控件
  operation_layout->addSpacing(10);
  cbx_mouse_add_point_ = new QCheckBox(this);
  cbx_mouse_add_point_->setText("add path");
  operation_layout->addWidget(cbx_mouse_add_point_);

  // End path button
  btn_end_path_ = new QPushButton("End Path", this);
  operation_layout->addWidget(btn_end_path_);

  btn_clear_all_path_ = new QPushButton("clear all", this);
  operation_layout->addWidget(btn_clear_all_path_);

  // 路径列表容器
  paths_list_widget_ = new QWidget(this);
  paths_list_layout_ = new QVBoxLayout(paths_list_widget_);
  paths_list_layout_->setContentsMargins(2, 2, 2, 2);
  paths_list_layout_->setSpacing(4);
  operation_layout->addWidget(new QLabel("Saved Paths:", this));
  operation_layout->addWidget(paths_list_widget_);
  operation_layout->addStretch(5);
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
  connect(w3d_widget_, &Map3DWidget::PathSaved, this,
          &MapMainWidget::SlotOnPathSaved);
  connect(w3d_widget_, &Map3DWidget::PathRemoved, this,
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

  connect(slider_path_width_, &QSlider::valueChanged, this,
          &MapMainWidget::SlotPathWidthChanged);
  connect(btn_path_color_, &QPushButton::clicked, this,
          &MapMainWidget::SlotPickPathColor);
  connect(cbx_apply_all_paths_, &QCheckBox::stateChanged, this,
          &MapMainWidget::SlotApplyAllPathsStyleChanged);
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
  if (btn_enter_3d_->text() == "enter 3d") {
    if (map_view_) {
      map_view_->SetAddingPath(enable);
    }
  } else {
    if (w3d_widget_) {
      w3d_widget_->SetAddingPath(enable);
    }
  }
}

void MapMainWidget::SlotClearAllPath() {
  if (map_view_) {
    if (btn_enter_3d_->text() == "enter 3d") {
      if (map_view_) {
        map_view_->ClearAllPaths();
      }
    } else {
      if (w3d_widget_) {
        w3d_widget_->ClearAllPaths();
      }
    }

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
  quint64 id;
  if (btn_enter_3d_->text() == "enter 3d") {
    if (map_view_) {
      id = map_view_->SaveCurrentPath(QString());
    }
  } else {
    if (w3d_widget_) {
      id = w3d_widget_->SaveCurrentPath(QString());
    }
  }

  Q_UNUSED(id);
  cbx_mouse_add_point_->setChecked(false);
  // PathSaved 信号会通知并添加侧边 UI
}

void MapMainWidget::SlotOnPathSaved(quint64 id, const QString& name,
                                    double lengthMeters) {
  // 新建一行： label + delete button
  QWidget* row = new QWidget(paths_list_widget_);
  row->setObjectName("pathRow");

  QHBoxLayout* hl = new QHBoxLayout(row);
  hl->setContentsMargins(2, 2, 2, 2);

  QLabel* lbl =
      new QLabel(QString("%1    <span style='color:#47A9FF;'>%2</span>")
                     .arg(name, FormatLength(lengthMeters)),
                 row);
  lbl->setTextFormat(Qt::RichText);
  lbl->setToolTip(QString("%1 — %2 m").arg(name).arg(lengthMeters, 0, 'f', 2));

  QPushButton* del = new QPushButton("Delete", row);
  del->setObjectName("btnDelete");
  hl->addWidget(lbl);
  hl->addStretch();
  hl->addWidget(del);
  paths_list_layout_->addWidget(row);

  connect(del, &QPushButton::clicked, this, [this, id, row]() {
    if (btn_enter_3d_->text() == "enter 3d") {
      if (map_view_) map_view_->RemoveSavedPathById(id);
    } else {
      if (w3d_widget_) w3d_widget_->RemoveSavedPathById(id);
    }
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
  ApplyPathStyle();
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

void MapMainWidget::SlotPathWidthChanged(int width) {
  path_width_ = width;
  label_path_width_->setText(QString::number(width));
  ApplyPathStyle();
}

void MapMainWidget::SlotPickPathColor() {
  QColor c = QColorDialog::getColor(path_color_, this, "Path Color",
                                    QColorDialog::ShowAlphaChannel);
  if (!c.isValid()) return;
  path_color_ = c;
  UpdatePathColorButton();
  ApplyPathStyle();
}

void MapMainWidget::SlotApplyAllPathsStyleChanged(int state) {
  if (state != 0) ApplyPathStyle();  // 勾选瞬间同步已有路径
}

void MapMainWidget::ApplyPathStyle() {
  const bool all = cbx_apply_all_paths_->isChecked();

  // 两个视图都下发，切换 2D/3D 后样式保持一致
  if (map_view_) {
    if (all)
      map_view_->SetAllPathsStyle(path_width_, path_color_);
    else
      map_view_->SetPathStyle(path_width_, path_color_);
  }
  if (w3d_widget_) {
    if (all)
      w3d_widget_->SetAllPathsStyle(path_width_, path_color_);
    else
      w3d_widget_->SetPathStyle(path_width_, path_color_);
  }
}

void MapMainWidget::UpdatePathColorButton() {
  btn_path_color_->setStyleSheet(
      QString("QPushButton#btnPathColor{"
              "background-color:%1;"
              "border:1px solid #454B55;"
              "border-radius:4px;}"
              "QPushButton#btnPathColor:hover{border-color:#47A9FF;}")
          .arg(path_color_.name(QColor::HexArgb)));
}