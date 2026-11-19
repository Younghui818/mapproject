// CameraController.cpp
#include "CameraController.h"
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector3D>
#include <QQuaternion>
#include <QWindow>

CameraController::CameraController(Qt3DRender::QCamera* camera, QWindow* window, QObject* parent)
    : QObject(parent), camera_(camera), window_(window) {
  window->installEventFilter(this);
}

bool CameraController::eventFilter(QObject* obj, QEvent* event) {
  if (obj != window_) return false;

  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      QMouseEvent* me = static_cast<QMouseEvent*>(event);
      last_pos_ = me->pos();
      buttons_ = me->buttons();
      return true;  // 吃掉事件，不让 Qt3D 默认处理
    }
    case QEvent::MouseMove: {
      QMouseEvent* me = static_cast<QMouseEvent*>(event);
      QPoint delta = me->pos() - last_pos_;
      last_pos_ = me->pos();

      if (buttons_ & Qt::LeftButton) {
        RotateCamera(delta);
      } else if (buttons_ & Qt::RightButton || buttons_ & Qt::MiddleButton) {
        PanCamera(delta);
      }
      return true;
    }
    case QEvent::MouseButtonRelease: {
      buttons_ = Qt::NoButton;
      return true;
    }
    case QEvent::Wheel: {
      QWheelEvent* we = static_cast<QWheelEvent*>(event);
      float delta = we->angleDelta().y();
      float dist = camera_->position().length();

      // 缩放：滚轮向上拉近，向下推远
      float factor = (delta > 0) ? (1.0f / zoom_factor_) : zoom_factor_;

      // 限制最小/最大距离
      QVector3D newPos = camera_->position() * factor;
      if (newPos.length() < 0.5f) factor = 0.5f / camera_->position().length();
      if (newPos.length() > 500.0f) factor = 500.0f / camera_->position().length();

      camera_->setPosition(camera_->position() * factor);
      return true;
    }
    default:
      break;
  }
  return false;
}

void CameraController::RotateCamera(const QPoint& delta) {
  // 绕 viewCenter 轨道旋转
  QVector3D center = camera_->viewCenter();
  QVector3D pos = camera_->position();
  QVector3D offset = pos - center;

  // 水平旋转（绕 up 轴）
  QQuaternion rotY = QQuaternion::fromAxisAndAngle(camera_->upVector(), -delta.x() * rotate_speed_);
  // 垂直旋转（绕 right 轴）
  QVector3D right = QVector3D::crossProduct(offset, camera_->upVector()).normalized();
  QQuaternion rotX = QQuaternion::fromAxisAndAngle(right, -delta.y() * rotate_speed_);

  offset = rotY * rotX * offset;
  camera_->setPosition(center + offset);
  camera_->setViewCenter(center);
}

void CameraController::PanCamera(const QPoint& delta) {
  QVector3D center = camera_->viewCenter();
  QVector3D pos = camera_->position();

  // 从相机指向目标的向量（视线方向）
  QVector3D forward = center - pos;
  float len = forward.length();
  if (len < 1e-6f) return;  // 防止除零

  QVector3D up = camera_->upVector();

  // 计算相机的右向量：forward × up
  // 注意顺序：forward(指向目标) × up = 右向量（右手系）
  QVector3D right = QVector3D::crossProduct(forward, up);
  if (right.lengthSquared() < 1e-6f) {
    // 视线与 up 平行（正上/正下方），无法计算右向量
    // 此时用世界坐标系的右向量 (1,0,0) 作为 fallback
    right = QVector3D(1, 0, 0);
  } else {
    right.normalize();
  }

  // 鼠标向右拖 → 场景向右走 → 相机向左走（-right）
  // 鼠标向上拖 → 场景向上走 → 相机向下走（-up，因为 delta.y 为负时 +up*负 = -up）
  QVector3D translation = (-right * delta.x() + up * delta.y()) * pan_speed_;

  camera_->setPosition(pos + translation);
  camera_->setViewCenter(center + translation);
}