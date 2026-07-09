#pragma once

#include "math/Vec2.h"
#include "math/Vec3.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

struct Framebuffer {
    static constexpr int kMsaaSamples = 4;

    int width = 0;
    int height = 0;
    bool msaaEnabled = true;

    std::vector<uint32_t> color;
    std::vector<float> depth;

    // MSAA：每像素 4 个子采样（2×2）
    std::vector<Vec3> sampleColor;
    std::vector<float> sampleDepth;
    std::vector<uint8_t> sampleValid;

    static const std::array<Vec2, kMsaaSamples>& msaaOffsets();

    void resize(int w, int h) {
        width = w;
        height = h;
        color.assign(static_cast<size_t>(w * h), 0xFF404060);
        depth.assign(static_cast<size_t>(w * h), std::numeric_limits<float>::infinity());

        const size_t sampleCount = static_cast<size_t>(w * h * kMsaaSamples);
        sampleColor.assign(sampleCount, Vec3{0.25f, 0.25f, 0.38f});
        sampleDepth.assign(sampleCount, std::numeric_limits<float>::infinity());
        sampleValid.assign(sampleCount, 0);
    }

    void clear(uint32_t c = 0xFF404060) {
        std::fill(color.begin(), color.end(), c);
        std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::infinity());

        const Vec3 bg = unpackColor(c);
        std::fill(sampleColor.begin(), sampleColor.end(), bg);
        std::fill(sampleDepth.begin(), sampleDepth.end(), std::numeric_limits<float>::infinity());
        std::fill(sampleValid.begin(), sampleValid.end(), static_cast<uint8_t>(0));
    }

    bool inBounds(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    // 【重点】Z-Buffer：仅当新片段更近时才写入颜色
    void putPixel(int x, int y, uint32_t rgba, float z) {
        if (!inBounds(x, y)) return;
        const size_t idx = static_cast<size_t>(y * width + x);
        if (z < depth[idx]) {
            depth[idx] = z;
            color[idx] = rgba;
        }
    }

    void putSample(int x, int y, int sample, const Vec3& rgb, float z) {
        if (!inBounds(x, y) || sample < 0 || sample >= kMsaaSamples) return;
        const size_t idx = static_cast<size_t>(y * width + x) * kMsaaSamples + sample;
        if (z < sampleDepth[idx]) {
            sampleDepth[idx] = z;
            sampleColor[idx] = rgb;
            sampleValid[idx] = 1;
        }
    }

    // MSAA resolve：对每个像素的有效子采样取平均
    void resolveMsaa() {
        if (!msaaEnabled) return;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Vec3 accum{};
                int count = 0;
                const size_t base =
                    static_cast<size_t>(y * width + x) * static_cast<size_t>(kMsaaSamples);
                for (int s = 0; s < kMsaaSamples; ++s) {
                    if (sampleValid[base + s]) {
                        accum = accum + sampleColor[base + s];
                        ++count;
                    }
                }

                const size_t idx = static_cast<size_t>(y * width + x);
                if (count > 0) {
                    color[idx] = packColor(accum / static_cast<float>(count));
                    depth[idx] = sampleDepth[base];
                    for (int s = 1; s < kMsaaSamples; ++s) {
                        if (sampleValid[base + s] && sampleDepth[base + s] < depth[idx]) {
                            depth[idx] = sampleDepth[base + s];
                        }
                    }
                } else {
                    color[idx] = 0xFF888890;
                    depth[idx] = std::numeric_limits<float>::infinity();
                }
            }
        }
    }

    static uint32_t packColor(const Vec3& rgb) {
        const auto c = [](float v) {
            v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
            return static_cast<uint32_t>(v * 255.f + 0.5f);
        };
        return 0xFF000000u | (c(rgb.x) << 16) | (c(rgb.y) << 8) | c(rgb.z);
    }

    static Vec3 unpackColor(uint32_t rgba) {
        return {
            static_cast<float>((rgba >> 16) & 0xFF) / 255.f,
            static_cast<float>((rgba >> 8) & 0xFF) / 255.f,
            static_cast<float>(rgba & 0xFF) / 255.f,
        };
    }
};

inline const std::array<Vec2, Framebuffer::kMsaaSamples>& Framebuffer::msaaOffsets() {
    static const std::array<Vec2, kMsaaSamples> offsets = {
        Vec2{0.25f, 0.25f},
        Vec2{0.75f, 0.25f},
        Vec2{0.25f, 0.75f},
        Vec2{0.75f, 0.75f},
    };
    return offsets;
}
