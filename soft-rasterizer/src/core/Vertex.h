#pragma once

#include "math/Vec2.h"
#include "math/Vec3.h"

// 【重点】顶点 = 位置 + 属性（法线、切线、UV…）
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec3 tangent;  // 【拓展】切线空间 X 轴，与法线、副切线构成 TBN
    Vec3 color;
    Vec2 uv;
};
