#include "Mat4.h"

#include <cmath>

Mat4 Mat4::identity() {
    Mat4 r{};
    r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
    return r;
}

Mat4 Mat4::translate(const Vec3& t) {
    Mat4 r = identity();
    r.m[0][3] = t.x;
    r.m[1][3] = t.y;
    r.m[2][3] = t.z;
    return r;
}

Mat4 Mat4::scale(const Vec3& s) {
    Mat4 r = identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
}

Mat4 Mat4::rotateY(float radians) {
    Mat4 r = identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    r.m[0][0] = c;
    r.m[0][2] = s;
    r.m[2][0] = -s;
    r.m[2][2] = c;
    return r;
}

Mat4 Mat4::rotateX(float radians) {
    Mat4 r = identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    r.m[1][1] = c;
    r.m[1][2] = -s;
    r.m[2][1] = s;
    r.m[2][2] = c;
    return r;
}

Mat4 Mat4::perspective(float fovY, float aspect, float nearZ, float farZ) {
    // 【重点】透视矩阵把视锥体压进 CVV 立方体，同时让远处物体变小（w 与 z 相关）
    Mat4 r{};
    const float tanHalf = std::tan(fovY * 0.5f);
    r.m[0][0] = 1.f / (aspect * tanHalf);
    r.m[1][1] = 1.f / tanHalf;
    r.m[2][2] = -(farZ + nearZ) / (farZ - nearZ);
    r.m[2][3] = -(2.f * farZ * nearZ) / (farZ - nearZ);
    r.m[3][2] = -1.f;  // 使 clip.w = -viewZ
    return r;
}

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 f = (target - eye).normalized();
    const Vec3 r = f.cross(up).normalized();
    const Vec3 u = r.cross(f);

    Mat4 rot = identity();
    rot.m[0][0] = r.x;
    rot.m[1][0] = r.y;
    rot.m[2][0] = r.z;
    rot.m[0][1] = u.x;
    rot.m[1][1] = u.y;
    rot.m[2][1] = u.z;
    rot.m[0][2] = -f.x;
    rot.m[1][2] = -f.y;
    rot.m[2][2] = -f.z;

    return rot * translate({-eye.x, -eye.y, -eye.z});
}

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.m[row][col] = m[row][0] * o.m[0][col] + m[row][1] * o.m[1][col] +
                            m[row][2] * o.m[2][col] + m[row][3] * o.m[3][col];
        }
    }
    return r;
}

Vec4 Mat4::transformPoint(const Vec4& v) const {
    return {
        m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
        m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
        m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
        m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w,
    };
}
