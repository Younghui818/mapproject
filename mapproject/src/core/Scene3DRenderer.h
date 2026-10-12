#pragma once
#include <QWidget>
#include <QImage>
#include "MeshBuilder.h"

class QLabel;

class Scene3DRenderer : public QWidget {
  Q_OBJECT
public:
  explicit Scene3DRenderer(QWidget* parent = nullptr);
  void SetMesh(const MeshData& mesh);

signals:
  void ClickedAt(double x, double y, double z);

private:
  QLabel* imgLabel_;
};