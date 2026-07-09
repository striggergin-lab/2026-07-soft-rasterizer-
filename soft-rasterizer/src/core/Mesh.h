#pragma once

#include "Vertex.h"

#include <vector>

// Mesh = 顶点池 + 索引表。索引复用顶点，减少重复数据。
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    static Mesh createCube(float size = 1.f);
    static Mesh createFloor(float size = 10.f);
    static Mesh createSphere(float radius = 0.15f, int slices = 16, int stacks = 12);
};
