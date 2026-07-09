#pragma once

#include "math/Mat4.h"
#include "math/Vec3.h"

// 轨道相机：围绕 target 旋转，始终 lookAt(target)
struct Camera {
    Vec3 position{};
    Vec3 target{0.f, 0.f, 0.f};

    float yaw = 0.785398f;    // 弧度，绕 Y 轴
    float pitch = 0.615479f;  // 弧度，仰角（>0 相机在目标上方）
    float distance = 3.6f;

    float orbitSpeed = 0.006f;
    float panSpeed = 0.002f;
    float zoomSpeed = 0.35f;
    float minDistance = 1.5f;
    float maxDistance = 30.f;

    Mat4 viewMatrix() const;
    void syncPosition();
    void setView(const Vec3& focus, const Vec3& cameraPos);

    void orbit(float dx, float dy);
    void pan(float dx, float dy);
    void scrollZoom(float wheelDelta);
};
