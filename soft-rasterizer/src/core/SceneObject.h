#pragma once

#include "Mesh.h"
#include "math/Mat4.h"
#include "math/Vec3.h"

enum class ObjectMaterial { Textured, Flat, Emissive };

struct SceneObject {
    const Mesh* mesh = nullptr;
    Mat4 model = Mat4::identity();
    bool castShadow = true;
    bool receiveShadow = true;
    ObjectMaterial material = ObjectMaterial::Textured;
    Vec3 flatColor{0.72f, 0.72f, 0.75f};
    Vec3 emissiveColor{1.f, 0.92f, 0.65f};
    float alpha = 1.f;  // 1=不透明，<1 启用 Alpha 混合
    float castShadowStrength = 1.f;  // 投影强度，半透明物体可 <1 减淡阴影
};
