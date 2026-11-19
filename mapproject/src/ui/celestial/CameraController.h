#ifndef CAMERA_CONTROLLER_H_
#define CAMERA_CONTROLLER_H_

#include <QObject>
#include <QPoint>
#include <Qt3DRender/QCamera>

class QWindow;

class CameraController : public QObject {
  Q_OBJECT
 public:
  explicit CameraController(Qt3DRender::QCamera* camera, QWindow* window, QObject* parent = nullptr);

  // 可调参数
  void SetZoomFactor(float factor) { zoom_factor_ = factor; }  // 滚轮缩放幅度，默认 1.15
  void SetPanSpeed(float speed) { pan_speed_ = speed; }        // 平移速度，默认 0.05
  void SetRotateSpeed(float speed) { rotate_speed_ = speed; }  // 旋转速度，默认 0.3

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void RotateCamera(const QPoint& delta);
  void PanCamera(const QPoint& delta);

  Qt3DRender::QCamera* camera_;
  QWindow* window_;
  QPoint last_pos_;
  Qt::MouseButtons buttons_;

  float zoom_factor_ = 1.15f;  // 每次滚轮缩放 15%
  float pan_speed_ = 0.05f;
  float rotate_speed_ = 0.3f;
};
#endif  // CAMERA_CONTROLLER_H_