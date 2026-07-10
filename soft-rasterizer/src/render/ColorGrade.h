#pragma once

#include "core/Framebuffer.h"
#include "math/Vec3.h"

struct ColorGrade {
    float brightness = 0.f;
    float saturation = 1.f;
    float contrast = 1.f;

    void apply(Framebuffer& fb) const;

    static float sliderToBrightness(int pos);
    static float sliderToSaturation(int pos);
    static float sliderToContrast(int pos);
};
