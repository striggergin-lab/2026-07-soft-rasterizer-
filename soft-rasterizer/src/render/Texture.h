#pragma once

#include "math/Vec3.h"

#include <string>
#include <vector>

// CPU 纹理：支持 PNG 加载 + 双线性采样
struct Texture {
    int width = 0;
    int height = 0;
    std::vector<Vec3> pixels;

    static Texture createCheckerboard(int size = 64, int cells = 8);
    static Texture createFlatNormal();

    bool loadFromFile(const std::string& path, int maxDimension = 1024);
    bool valid() const { return width > 0 && height > 0 && !pixels.empty(); }

    Vec3 at(int x, int y) const;
    Vec3 sampleBilinear(float u, float v) const;

    // 【拓展】法线贴图：RGB → 切线空间法线 [-1,1]
    Vec3 sampleNormalMap(float u, float v, bool flipY = false) const;
};
