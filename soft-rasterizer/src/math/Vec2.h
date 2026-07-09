#pragma once

#include <cmath>

struct Vec2 {
    float x = 0.f;
    float y = 0.f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }

    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float length() const { return std::sqrt(dot(*this)); }
    Vec2 normalized() const {
        const float len = length();
        return len > 1e-6f ? Vec2{x / len, y / len} : Vec2{};
    }
};
