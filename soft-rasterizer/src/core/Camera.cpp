#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

}  // namespace

Mat4 Camera::viewMatrix() const {
    return Mat4::lookAt(position, target, Vec3{0.f, 1.f, 0.f});
}

void Camera::syncPosition() {
    distance = std::max(minDistance, std::min(maxDistance, distance));
    pitch = std::max(-1.55f, std::min(1.55f, pitch));

    const float cp = std::cos(pitch);
    const float sy = std::sin(yaw);
    const float cy = std::cos(yaw);
    const Vec3 offset{sy * cp, std::sin(pitch), cy * cp};
    position = target + offset * distance;
}

void Camera::setView(const Vec3& focus, const Vec3& cameraPos) {
    target = focus;
    position = cameraPos;
    const Vec3 offset = position - target;
    distance = offset.length();
    if (distance < 1e-6f) {
        distance = minDistance;
        syncPosition();
        return;
    }

    const Vec3 dir = offset / distance;
    pitch = std::asin(std::max(-1.f, std::min(1.f, dir.y)));
    const float cp = std::cos(pitch);
    if (std::abs(cp) > 1e-6f) {
        yaw = std::atan2(dir.x / cp, dir.z / cp);
    }
}

void Camera::orbit(float dx, float dy) {
    yaw += dx * orbitSpeed;
    pitch -= dy * orbitSpeed;
    syncPosition();
}

void Camera::pan(float dx, float dy) {
    const Vec3 f = (target - position).normalized();
    Vec3 right = f.cross({0.f, 1.f, 0.f});
    if (right.length() < 1e-6f) {
        right = {1.f, 0.f, 0.f};
    } else {
        right = right.normalized();
    }

    const float scale = distance * panSpeed;
    target = target - right * (dx * scale) + Vec3{0.f, 1.f, 0.f} * (dy * scale);
    syncPosition();
}

void Camera::scrollZoom(float wheelDelta) {
    distance -= (wheelDelta / 120.f) * zoomSpeed;
    syncPosition();
}
