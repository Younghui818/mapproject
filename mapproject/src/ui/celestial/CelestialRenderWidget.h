#pragma once
#include <QWidget>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QColor>
#include <Qt3DExtras/Qt3DWindow>

namespace Qt3DCore {
class QEntity;
class QTransform;
}  // namespace Qt3DCore

// 天体配置结构体
struct PlanetConfig {
  QString name;
  float radius;
  float orbitRadius;
  float axialTilt;
  QString texture;  // 新增：贴图路径
  QColor fallbackColor;

  // moon（radius/orbitRadius 为 0 表示不创建）
  float moonRadius = 0.0f;
  float moonOrbitRadius = 0.0f;
  float moonOrbitSpeed = 0.0f;  // 度/帧
  //  santurn_rings（inner/outer 为 0 表示不创建）
  float ringInner = 0.0f;
  float ringOuter = 0.0f;
};

// 行星动画状态结构体
struct PlanetState {
  Qt3DCore::QEntity* pivot_entity = nullptr;
  Qt3DCore::QTransform* pivot_transform = nullptr;

  // 倾斜节点，用于固定黄赤交角
  Qt3DCore::QEntity* tilt_entity = nullptr;
  Qt3DCore::QTransform* tilt_transform = nullptr;

  Qt3DCore::QEntity* planet_entity = nullptr;
  Qt3DCore::QTransform* planet_transform = nullptr;
  // 轨道
  Qt3DCore::QEntity* planet_orbit_entity = nullptr;

  float orbit_speed = 0.0f;
  float spin_speed = 0.0f;
  float current_angle = 0.0f;
  float current_spin = 0.0f;
  QVector<Qt3DCore::QEntity*> extra_visible_entities;

  // moon
  Qt3DCore::QEntity* moon_anchor = nullptr;
  Qt3DCore::QEntity* moon_pivot = nullptr;
  Qt3DCore::QTransform* moon_pivot_transform = nullptr;
  Qt3DCore::QEntity* moon_entity = nullptr;
  Qt3DCore::QTransform* moon_transform = nullptr;
  float moon_angle = 0.0f;
  float moon_speed = 0.0f;

  // santurn_rings
  Qt3DCore::QEntity* ring_entity = nullptr;
};

class CelestialRenderWidget : public QWidget {
  Q_OBJECT
 public:
  explicit CelestialRenderWidget(QWidget* parent = nullptr);
  ~CelestialRenderWidget() override = default;

 public slots:
  void SlotPlanetVisible(int index, bool visible);
  void SlotUpdateSpinAnimation();
  void SlotUpdateOrbitAnimation();
  void SlotStartSpinAnimation();
  void SlotStartOrbitAnimation();

 private:
  void InitUI();
  void Init3DScene();
  Qt3DCore::QEntity* CreateSun(Qt3DCore::QEntity* root);
  PlanetState CreatePlanet(const PlanetConfig& config, Qt3DCore::QEntity* root);
  void CreatPlanetRings(const PlanetConfig& config, PlanetState& state);
  Qt3DCore::QEntity* CreateRing(float innerR, float outerR, Qt3DCore::QEntity* parent, const QString& texturePath);

  Qt3DExtras::Qt3DWindow* view_ = nullptr;
  Qt3DCore::QEntity* root_entity_ = nullptr;
  Qt3DCore::QEntity* sun_entity_ = nullptr;
  QHash<QString, PlanetState> planets_;
  QTimer* animation_spin_timer_ = nullptr;
  QTimer* animation_orbit_timer_ = nullptr;
};