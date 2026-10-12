#include "Scene3DRenderer.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QPixmap>

Scene3DRenderer::Scene3DRenderer(QWidget* parent) : QWidget(parent) {
  auto* l = new QVBoxLayout(this);
  l->setContentsMargins(0,0,0,0);
  imgLabel_ = new QLabel(this);
  imgLabel_->setAlignment(Qt::AlignCenter);
  l->addWidget(imgLabel_);
}

void Scene3DRenderer::SetMesh(const MeshData& mesh) {
  // 原型：显示贴图作为占位，真实 3D 渲染在后续替换
  if (!mesh.texture.isNull()) {
    imgLabel_->setPixmap(QPixmap::fromImage(mesh.texture).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    imgLabel_->setText(QString("Mesh: vertices=%1 triangles=%2").arg(mesh.vertices.size()).arg(mesh.indices.size()/3));
  }
}