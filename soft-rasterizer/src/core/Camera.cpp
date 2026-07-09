#include "Camera.h"

#include <algorithm>
#include <cmath>

Mat4 Camera::viewMatrix() const {
    return Mat4::lookAt(position, Vec3{0.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f});
}

Vec3 Camera::forward() const {
    const float cp = std::cos(pitch);
    Vec3 f{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
    return f.normalized();
}

Vec3 Camera::right() const {
    return forward().cross({0.f, 1.f, 0.f}).normalized();
}

Vec3 Camera::up() const {
    return right().cross(forward()).normalized();
}

void Camera::syncOrbitPosition() {
    // 相机在视线反方向，距离 orbitRadius，始终朝向原点
    position = forward() * (-orbitRadius);
}

void Camera::rotate(float dx, float dy) {
    yaw += dx * rotateSpeed;
    pitch += dy * rotateSpeed;
    const float limit = 1.55f;
    pitch = std::max(-limit, std::min(limit, pitch));
    syncOrbitPosition();
}

void Camera::scrollZoom(float wheelDelta) {
    // 滚轮向上（正值）推远，向下（负值）拉近
    orbitRadius += (wheelDelta / 120.f) * 0.35f;
    orbitRadius = std::max(0.5f, std::min(50.f, orbitRadius));
    syncOrbitPosition();
}

void Camera::moveRight(float dt) {
    yaw -= dt * moveSpeed * 0.5f;
    syncOrbitPosition();
}

void Camera::moveUp(float dt) {
    pitch += dt * moveSpeed * 0.5f;
    const float limit = 1.55f;
    pitch = std::max(-limit, std::min(limit, pitch));
    syncOrbitPosition();
}
