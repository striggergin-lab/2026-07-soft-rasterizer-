#pragma once

#include "Vec3.h"

// 【重点】齐次坐标：用 (x,y,z,w) 表示 3D 点/向量
// - 点：w=1，平移会生效
// - 方向向量：w=0，平移不影响
// - 透视投影后 w != 1，需除以 w 得 NDC；插值时用 1/w 做透视矫正
struct Vec4 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float w = 1.f;

    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_ = 1.f) : x(x_), y(y_), z(z_), w(w_) {}
    explicit Vec4(const Vec3& v, float w_ = 1.f) : x(v.x), y(v.y), z(v.z), w(w_) {}

    Vec3 xyz() const { return {x, y, z}; }
    Vec3 perspectiveDivide() const {
        if (std::abs(w) < 1e-6f) return {x, y, z};
        return {x / w, y / w, z / w};
    }
};
