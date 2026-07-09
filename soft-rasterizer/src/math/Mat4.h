#pragma once

#include "Vec3.h"
#include "Vec4.h"

struct Mat4 {
    float m[4][4] = {};

    static Mat4 identity();
    static Mat4 translate(const Vec3& t);
    static Mat4 scale(const Vec3& s);
    static Mat4 rotateY(float radians);
    static Mat4 rotateX(float radians);
    static Mat4 perspective(float fovY, float aspect, float nearZ, float farZ);
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up);

    Mat4 operator*(const Mat4& o) const;
    Vec4 transformPoint(const Vec4& v) const;
};
