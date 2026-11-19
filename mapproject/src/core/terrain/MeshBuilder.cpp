#include "MeshBuilder.h"
#include <cmath>

MeshBuilder::MeshBuilder(QObject* parent) : QObject(parent) {}

static QVector3D ComputeNormal(const QVector3D& a, const QVector3D& b, const QVector3D& c) {
  // 三角形法线由两条边的叉积得到；单位化后便于后续光照计算。
  return QVector3D::crossProduct(b - a, c - a).normalized();
}

MeshData MeshBuilder::BuildMesh(const DemProcessor::HeightGrid& grid, const QImage& texture, double gridSpacingMeters) {
  MeshData out;
  if (grid.width <= 1 || grid.height <= 1) return out;
  int w = grid.width;
  int h = grid.height;
  out.vertices.reserve(w * h);
  out.normals.resize(w * h);
  // 每个高程采样点对应一个三维顶点：X/Y 是平面位置，Z 是高程值。
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float hgt = grid.heights[y * w + x];
      out.vertices.emplace_back(x * gridSpacingMeters, y * gridSpacingMeters, hgt);
    }
  }
  // 一个规则网格单元拆成两个三角形。索引顺序决定正面朝向，
  // 这里采用逆时针绕序，使法线朝向地形上方。
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
  // 将相邻三角形法线累加到顶点，再统一归一化，形成平滑着色所需的顶点法线。
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