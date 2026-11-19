#include "CelestialOperationWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>

const QVector<QString> kPlanetNames = {"Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};

CelestialOperationWidget::CelestialOperationWidget(QWidget* parent) : QWidget(parent) {
  InitUI();
  SingalLink();
}

void CelestialOperationWidget::InitUI() {
  QVBoxLayout* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);

  QHBoxLayout* btn_layout = new QHBoxLayout(this);
  btn_layout->setContentsMargins(0, 0, 0, 0);
  btn_spin_start_ = new QPushButton("startSpin", this);
  btn_layout->addWidget(btn_spin_start_);
  btn_orbit_start_ = new QPushButton("startOrbit", this);
  btn_layout->addWidget(btn_orbit_start_);

  main_layout->addLayout(btn_layout);

  cb_sun_ = new QCheckBox("Sun", this);
  main_layout->addWidget(cb_sun_);

  cb_all_planets_ = new QCheckBox("All Planets", this);
  main_layout->addWidget(cb_all_planets_);

  main_layout->addWidget(InitPlanetVisibleWidget());

  main_layout->addStretch(5);
}

QWidget* CelestialOperationWidget::InitPlanetVisibleWidget() {
  QWidget* widget_planes_visible = new QWidget(this);

  QGridLayout* planets_layout = new QGridLayout(widget_planes_visible);
  planets_layout->setContentsMargins(0, 0, 0, 0);
  const int columns_per_row = 3;
  for (int i = 0; i < kPlanetNames.size(); i++) {
    QCheckBox* cb = new QCheckBox(kPlanetNames[i], widget_planes_visible);
    int row = i / columns_per_row;
    int col = i % columns_per_row;
    planets_layout->addWidget(cb, row, col);
    cb_planets_.append(cb);
  }
  return widget_planes_visible;
}

void CelestialOperationWidget::SingalLink() {
  connect(btn_spin_start_, &QPushButton::clicked, this, [this]() { emit SignalStartSpinAnimation(); });
  connect(btn_orbit_start_, &QPushButton::clicked, this, [this]() { emit SignalStartOrbitAnimation(); });
  connect(cb_sun_, &QCheckBox::toggled, this, [=](bool state) { emit SignalPlanetVisible(-1, state); });
  connect(cb_all_planets_, &QCheckBox::toggled, this, [=](bool state) {
    for (int i = 0; i < cb_planets_.size(); i++) {
      cb_planets_[i]->setChecked(state);
    }
  });
  for (int i = 0; i < cb_planets_.size(); i++) {
    connect(cb_planets_[i], &QCheckBox::toggled, this, [=](bool state) { emit SignalPlanetVisible(i, state); });
  }
}

void CelestialOperationWidget::SlotBtnStartClicked() {}