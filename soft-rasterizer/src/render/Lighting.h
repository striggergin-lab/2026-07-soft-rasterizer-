#pragma once

#include "math/Vec3.h"

struct DirectionalLight {
    Vec3 direction{-0.3f, -1.f, -0.4f};
    Vec3 color{1.f, 1.f, 1.f};
    float ambient = 0.25f;
    float diffuse = 0.75f;
    float specular = 0.35f;
    float shininess = 32.f;
};

// 【重点】Blinn-Phong 光照 + 法线贴图 TBN 变换
Vec3 blinnPhong(const Vec3& fragPos, const Vec3& normal, const Vec3& viewPos, const Vec3& albedo,
                const DirectionalLight& light);

// 切线空间法线 → 世界空间（TBN 矩阵）
Vec3 tangentToWorld(const Vec3& normalTangent, const Vec3& worldNormal, const Vec3& worldTangent);
