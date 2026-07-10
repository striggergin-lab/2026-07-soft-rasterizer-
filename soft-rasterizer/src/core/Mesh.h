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

    enum class WallFacing { PosX, PosZ };

    // 竖直墙面：宽×高，底边 y=0；PosX 沿 +Z 展开，PosZ 沿 +X 展开
    static Mesh createWall(float width, float height, WallFacing facing);
    // 居中镂空 winW×winH 矩形窗（横向窗：winW > winH），底边贴地
    static Mesh createWallWithWindow(float width, float height, float winW, float winH,
                                     WallFacing facing);
};
