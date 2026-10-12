#include "MeshBuilder.h"
#include <cmath>

MeshBuilder::MeshBuilder(QObject* parent) : QObject(parent) {}

static QVector3D ComputeNormal(const QVector3D& a, const QVector3D& b, const QVector3D& c) {
  return QVector3D::crossProduct(b - a, c - a).normalized();
}

MeshData MeshBuilder::BuildMesh(const DemProcessor::HeightGrid& grid, const QImage& texture, double gridSpacingMeters) {
  MeshData out;
  if (grid.width <= 1 || grid.height <= 1) return out;
  int w = grid.width;
  int h = grid.height;
  out.vertices.reserve(w * h);
  out.normals.resize(w * h);
  // create vertices
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float hgt = grid.heights[y * w + x];
      out.vertices.emplace_back(x * gridSpacingMeters, y * gridSpacingMeters, hgt);
    }
  }
  // indices (two triangles per cell)
  for (int y = 0; y < h - 1; ++y) {
    for (int x = 0; x < w - 1; ++x) {
      int i0 = y * w + x;
      int i1 = y * w + (x + 1);
      int i2 = (y + 1) * w + x;
      int i3 = (y + 1) * w + (x + 1);
      // tri 1: i0, i2, i1
      out.indices.push_back(i0);
      out.indices.push_back(i2);
      out.indices.push_back(i1);
      // tri 2: i1, i2, i3
      out.indices.push_back(i1);
      out.indices.push_back(i2);
      out.indices.push_back(i3);
    }
  }
  // normals: average face normals per vertex
  for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
    int ia = out.indices[i];
    int ib = out.indices[i + 1];
    int ic = out.indices[i + 2];
    QVector3D n = ComputeNormal(out.vertices[ia], out.vertices[ib], out.vertices[ic]);
    out.normals[ia] = out.normals[ia] + n;
    out.normals[ib] = out.normals[ib] + n;
    out.normals[ic] = out.normals[ic] + n;
  }
  for (auto& nv : out.normals) {
    nv.normalize();
  }
  out.texture = texture;
  return out;
}