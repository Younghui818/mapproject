#include "Map3DOperationWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

Map3DOperationWidget::Map3DOperationWidget(Map3DWidget* w3d_widget,
                                           QWidget* parent /* = nullptr */)
    : QWidget(parent), w3d_widget_(w3d_widget) {
  InitUI();
  SignalLink();

  btn_show_point_->setChecked(false);
  btn_show_mesh_->setChecked(true);
}

void Map3DOperationWidget::InitUI() {
  // Implementation for initializing the UI
  // 创建 3D 显示模式、垂直夸张、光照和亮度控制项。
  this->setObjectName("widget_3d_map_show");
  QVBoxLayout* widget_3d_map_show_layout = new QVBoxLayout(this);

  QWidget* widget_3d_show_type = new QWidget(this);
  QHBoxLayout* widget_3d_show_type_layout =
      new QHBoxLayout(widget_3d_show_type);
  widget_3d_show_type_layout->setContentsMargins(5, 5, 5, 5);
  btn_show_point_ = new QRadioButton("point", this);
  btn_show_mesh_ = new QRadioButton("mesh", this);
  widget_3d_show_type_layout->addWidget(btn_show_point_);
  widget_3d_show_type_layout->addWidget(btn_show_mesh_);
  widget_3d_map_show_layout->addWidget(widget_3d_show_type);

  cbx_show_cva_ = new QCheckBox(this);
  cbx_show_cva_->setText("show cva");
  cbx_show_cva_->setChecked(false);
  widget_3d_map_show_layout->addWidget(cbx_show_cva_);

  QWidget* widget_3d_height_scal = new QWidget(this);
  QHBoxLayout* widget_3d_height_scal_layout =
      new QHBoxLayout(widget_3d_height_scal);
  widget_3d_height_scal_layout->setContentsMargins(5, 5, 5, 5);
  slider_3d_height_scal_ = new QSlider(Qt::Orientation::Horizontal, this);
  slider_3d_height_scal_->setRange(0, 300);
  slider_3d_height_scal_->setValue(30);
  QLabel* label_3d_height_scal = new QLabel("height scale", this);
  widget_3d_height_scal_layout->addWidget(label_3d_height_scal);
  widget_3d_height_scal_layout->addWidget(slider_3d_height_scal_);
  edit_3d_height_scal_ = new QLabel(this);
  edit_3d_height_scal_->setText("3");
  widget_3d_height_scal_layout->addWidget(edit_3d_height_scal_);

  widget_3d_map_show_layout->addWidget(widget_3d_height_scal);

  widget_combo_preset_light_ = new QWidget(this);
  QHBoxLayout* widget_combo_preset_light_layout =
      new QHBoxLayout(widget_combo_preset_light_);
  QLabel* label_combo_preset_light = new QLabel("preset", this);
  widget_combo_preset_light_layout->addWidget(label_combo_preset_light);
  combo_preset_light_ = new QComboBox(this);
  combo_preset_light_->addItem("bright");
  combo_preset_light_->addItem("strong");
  combo_preset_light_->addItem("soft");
  combo_preset_light_->addItem("common");
  widget_combo_preset_light_layout->addWidget(combo_preset_light_);
  widget_3d_map_show_layout->addWidget(widget_combo_preset_light_);

  widget_sliderAmbient_ = new QWidget(this);
  QHBoxLayout* widget_sliderAmbient_layout =
      new QHBoxLayout(widget_sliderAmbient_);
  QLabel* label_sliderAmbient = new QLabel("Ambient", this);
  widget_sliderAmbient_layout->addWidget(label_sliderAmbient);
  sliderAmbient_ = new QSlider(Qt::Orientation::Horizontal, this);
  sliderAmbient_->setRange(0, 100);
  sliderAmbient_->setValue(50);
  widget_sliderAmbient_layout->addWidget(sliderAmbient_);
  widget_3d_map_show_layout->addWidget(widget_sliderAmbient_);

  widget_sliderAzimuth_ = new QWidget(this);
  QHBoxLayout* widget_sliderAzimuth_layout =
      new QHBoxLayout(widget_sliderAzimuth_);
  QLabel* label_sliderAzimuth = new QLabel("Azimuth", this);
  widget_sliderAzimuth_layout->addWidget(label_sliderAzimuth);
  sliderAzimuth_ = new QSlider(Qt::Orientation::Horizontal, this);
  sliderAzimuth_->setRange(0, 359);
  sliderAzimuth_->setValue(90);
  widget_sliderAzimuth_layout->addWidget(sliderAzimuth_);
  widget_3d_map_show_layout->addWidget(widget_sliderAzimuth_);

  widget_sliderElev_ = new QWidget(this);
  QHBoxLayout* widget_sliderElev_layout = new QHBoxLayout(widget_sliderElev_);
  QLabel* label_sliderElev = new QLabel("Elev", this);
  widget_sliderElev_layout->addWidget(label_sliderElev);
  sliderElev_ = new QSlider(Qt::Orientation::Horizontal, this);
  sliderElev_->setRange(5, 89);
  sliderElev_->setValue(50);
  widget_sliderElev_layout->addWidget(sliderElev_);
  widget_3d_map_show_layout->addWidget(widget_sliderElev_);

  widget_sliderBright_ = new QWidget(this);
  QHBoxLayout* widget_sliderBright_layout =
      new QHBoxLayout(widget_sliderBright_);
  QLabel* label_sliderBright = new QLabel("Bright", this);
  widget_sliderBright_layout->addWidget(label_sliderBright);
  sliderBright_ = new QSlider(Qt::Orientation::Horizontal, this);
  sliderBright_->setRange(20, 300);
  sliderBright_->setValue(100);
  widget_sliderBright_layout->addWidget(sliderBright_);
  widget_3d_map_show_layout->addWidget(widget_sliderBright_);
  this->hide();
}

void Map3DOperationWidget::SignalLink() {
  connect(btn_show_point_, &QRadioButton::clicked, this,
          &Map3DOperationWidget::SlotUpdate3DMapShow);
  connect(btn_show_mesh_, &QRadioButton::clicked, this,
          &Map3DOperationWidget::SlotUpdate3DMapShow);
  connect(slider_3d_height_scal_, &QSlider::valueChanged, this,
          &Map3DOperationWidget::SlotUpdate3DHeightScale);
  connect(cbx_show_cva_, &QCheckBox::checkStateChanged, this,
          &Map3DOperationWidget::SlotUpdateMapImageShow);

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

void Map3DOperationWidget::SlotUpdate3DMapShow() {
  // 将点云/网格单选状态转发给 3D 控件，并控制仅点云模式可用的光照滑块。
  // 切换3d显示模式
  if (w3d_widget_) {
    if (btn_show_point_->isChecked()) {
      w3d_widget_->UpdateMap3DShowType(kMap3DShowTypePoint);
    } else if (btn_show_mesh_->isChecked()) {
      if (cbx_show_cva_->isChecked()) {
        w3d_widget_->UpdateMap3DShowType(kMap3DShowTypeTexture);
      } else {
        w3d_widget_->UpdateMap3DShowType(kMap3DShowTypeMesh);
      }
    }
  }
  widget_sliderAmbient_->setVisible(btn_show_point_->isChecked());
  widget_sliderAzimuth_->setVisible(btn_show_point_->isChecked());
  widget_sliderElev_->setVisible(btn_show_point_->isChecked());
}

void Map3DOperationWidget::SlotUpdate3DHeightScale(int scale) {
  // 将滑块整数值转换为垂直夸张倍数，并刷新显示文本。
  if (w3d_widget_) {
    w3d_widget_->SetDemExaggeration(scale / 10);
    edit_3d_height_scal_->setText(QString::number(scale / 10.0, 'f', 1));
  }
}

void Map3DOperationWidget::SlotUpdateMapImageShow(bool show) {
  if (w3d_widget_) {
    w3d_widget_->UpdateMap3DShowType(show ? kMap3DShowTypeTexture
                                          : kMap3DShowTypeMesh);
  }
}

void Map3DOperationWidget::showEvent(QShowEvent* event) {
  // 判断亮度调节显示哪些内容
  widget_sliderAmbient_->setVisible(btn_show_point_->isChecked());
  widget_sliderAzimuth_->setVisible(btn_show_point_->isChecked());
  widget_sliderElev_->setVisible(btn_show_point_->isChecked());
  btn_show_point_->setChecked(true);
  QWidget::showEvent(event);
}