#pragma once

#include "math/Vec3.h"

// 【拓展】双线性插值：在 2×2 格子内按 (fx, fy) 混合四个角的值
// 纹理采样、MSAA 解算等场景都会用到
inline float bilinearLerp(float c00, float c10, float c01, float c11, float fx, float fy) {
    const float top = c00 + (c10 - c00) * fx;
    const float bot = c01 + (c11 - c01) * fx;
    return top + (bot - top) * fy;
}

inline Vec3 bilinearLerp(const Vec3& c00, const Vec3& c10, const Vec3& c01, const Vec3& c11,
                         float fx, float fy) {
    return {
        bilinearLerp(c00.x, c10.x, c01.x, c11.x, fx, fy),
        bilinearLerp(c00.y, c10.y, c01.y, c11.y, fx, fy),
        bilinearLerp(c00.z, c10.z, c01.z, c11.z, fx, fy),
    };
}
