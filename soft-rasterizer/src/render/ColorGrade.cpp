#include "ColorGrade.h"

#include <algorithm>

float ColorGrade::sliderToBrightness(int pos) {
    pos = std::clamp(pos, 0, 100);
    return (static_cast<float>(pos) - 50.f) / 50.f * 0.35f;
}

float ColorGrade::sliderToSaturation(int pos) {
    pos = std::clamp(pos, 0, 100);
    return static_cast<float>(pos) / 50.f;
}

float ColorGrade::sliderToContrast(int pos) {
    pos = std::clamp(pos, 0, 100);
    return static_cast<float>(pos) / 50.f;
}

void ColorGrade::apply(Framebuffer& fb) const {
    if (brightness == 0.f && saturation == 1.f && contrast == 1.f) {
        return;
    }

    for (uint32_t& px : fb.color) {
        Vec3 c = Framebuffer::unpackColor(px);

        c.x = (c.x - 0.5f) * contrast + 0.5f;
        c.y = (c.y - 0.5f) * contrast + 0.5f;
        c.z = (c.z - 0.5f) * contrast + 0.5f;

        const float lum = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
        c.x = lum + (c.x - lum) * saturation;
        c.y = lum + (c.y - lum) * saturation;
        c.z = lum + (c.z - lum) * saturation;

        c.x += brightness;
        c.y += brightness;
        c.z += brightness;

        c.x = std::clamp(c.x, 0.f, 1.f);
        c.y = std::clamp(c.y, 0.f, 1.f);
        c.z = std::clamp(c.z, 0.f, 1.f);
        px = Framebuffer::packColor(c);
    }
}
