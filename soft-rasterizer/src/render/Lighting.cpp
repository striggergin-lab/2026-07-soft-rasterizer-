#include "Lighting.h"

#include <algorithm>
#include <cmath>

Vec3 tangentToWorld(const Vec3& normalTangent, const Vec3& worldNormal, const Vec3& worldTangent) {
    Vec3 N = worldNormal.normalized();
    Vec3 T = worldTangent.normalized();
    T = (T - N * N.dot(T));
    if (T.length() < 1e-6f) {
        T = {1.f, 0.f, 0.f};
    }
    T = T.normalized();
    const Vec3 B = N.cross(T).normalized();

    // n_world = TBN * n_tangent
    return (T * normalTangent.x + B * normalTangent.y + N * normalTangent.z).normalized();
}

Vec3 blinnPhong(const Vec3& fragPos, const Vec3& normal, const Vec3& viewPos, const Vec3& albedo,
                const DirectionalLight& light) {
    const Vec3 n = normal.normalized();
    const Vec3 l = (-light.direction).normalized();
    const Vec3 v = (viewPos - fragPos).normalized();
    const Vec3 h = (l + v).normalized();

    const float ndotl = std::max(n.dot(l), 0.f);
    const float ndoth = std::max(n.dot(h), 0.f);

    const Vec3 ambient = albedo * light.ambient;
    const Vec3 diffuse = albedo * (light.diffuse * ndotl);
    const Vec3 spec = light.color * (light.specular * std::pow(ndoth, light.shininess));

    return ambient + diffuse + spec;
}

Vec3 blinnPhongPoint(const Vec3& fragPos, const Vec3& normal, const Vec3& viewPos, const Vec3& albedo,
                     const PointLight& light, float shadowFactor) {
    const Vec3 n = normal.normalized();
    const Vec3 toLight = light.position - fragPos;
    const float dist = toLight.length();
    const Vec3 l = toLight / (dist + 1e-6f);
    const Vec3 v = (viewPos - fragPos).normalized();
    const Vec3 h = (l + v).normalized();

    const float ndotl = std::max(n.dot(l), 0.f);
    const float ndoth = std::max(n.dot(h), 0.f);

    const float att = 1.f / (1.f + light.attenuation * dist * dist);

    const Vec3 ambient = albedo * light.ambient;
    const Vec3 diffuse = albedo * (light.diffuse * ndotl * att * shadowFactor);
    const Vec3 spec =
        light.color * (light.specular * std::pow(ndoth, light.shininess) * att * shadowFactor);

    return ambient + diffuse + spec;
}
