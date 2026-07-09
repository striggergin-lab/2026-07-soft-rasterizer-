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

struct PointLight {
    Vec3 position{3.f, 5.f, 3.f};
    Vec3 color{1.f, 0.98f, 0.92f};
    float ambient = 0.38f;
    float diffuse = 0.95f;
    float specular = 0.45f;
    float shininess = 48.f;
    float attenuation = 0.08f;
};

Vec3 blinnPhong(const Vec3& fragPos, const Vec3& normal, const Vec3& viewPos, const Vec3& albedo,
                const DirectionalLight& light);

Vec3 blinnPhongPoint(const Vec3& fragPos, const Vec3& normal, const Vec3& viewPos, const Vec3& albedo,
                     const PointLight& light, float shadowFactor = 1.f);

Vec3 tangentToWorld(const Vec3& normalTangent, const Vec3& worldNormal, const Vec3& worldTangent);
