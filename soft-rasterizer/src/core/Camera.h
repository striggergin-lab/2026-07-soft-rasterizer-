#pragma once

#include "math/Mat4.h"
#include "math/Vec3.h"

// 轨道相机：绕原点旋转，右键拖拽改变 yaw/pitch
struct Camera {
    Vec3 position{0.f, 0.f, 2.5f};
    float yaw = 3.14159265f;  // 默认从 +Z 侧看向原点
    float pitch = 0.f;
    float orbitRadius = 2.5f;
    float moveSpeed = 2.5f;
    float rotateSpeed = 0.01f;

    Mat4 viewMatrix() const;
    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;

    void rotate(float dx, float dy);
    void syncOrbitPosition();
    void scrollZoom(float wheelDelta);
    void moveRight(float dt);
    void moveUp(float dt);
};
