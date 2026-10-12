#pragma once
#include <QObject>
#include <QVector3D>
#include <QImage>
#include <vector>
#include "DemProcessor.h"

struct MeshData {
  std::vector<QVector3D> vertices;
  std::vector<QVector3D> normals;
  std::vector<int> indices;
  QImage texture;
};

class MeshBuilder : public QObject {
  Q_OBJECT
public:
  explicit MeshBuilder(QObject* parent = nullptr);
  MeshData BuildMesh(const DemProcessor::HeightGrid& grid, const QImage& texture, double gridSpacingMeters);
};