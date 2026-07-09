#pragma once

#include "core/Framebuffer.h"
#include "math/Vec2.h"

#include <cmath>
#include <limits>
#include <vector>

// 【拓展】Shadow Map：光源视角深度缓冲，支持 MSAA 子采样
struct ShadowMap {
    static constexpr int kMsaaSamples = Framebuffer::kMsaaSamples;

    int width = 1024;
    int height = 1024;
    bool msaaEnabled = true;

    std::vector<float> sampleDepth;
    std::vector<uint8_t> sampleValid;

    void resize(int w, int h) {
        width = w;
        height = h;
        const size_t count = static_cast<size_t>(w * h * kMsaaSamples);
        sampleDepth.assign(count, std::numeric_limits<float>::infinity());
        sampleValid.assign(count, 0);
    }

    void clear() {
        std::fill(sampleDepth.begin(), sampleDepth.end(), std::numeric_limits<float>::infinity());
        std::fill(sampleValid.begin(), sampleValid.end(), static_cast<uint8_t>(0));
    }

    bool inBounds(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    void putSample(int x, int y, int sample, float depth) {
        if (!inBounds(x, y) || sample < 0 || sample >= kMsaaSamples) return;
        const size_t idx = static_cast<size_t>(y * width + x) * kMsaaSamples + sample;
        if (depth < sampleDepth[idx]) {
            sampleDepth[idx] = depth;
            sampleValid[idx] = 1;
        }
    }

    float sampleDepthAt(float u, float v, int msaaSample) const {
        if (u < 0.f || u > 1.f || v < 0.f || v > 1.f) {
            return std::numeric_limits<float>::infinity();
        }

        float x = u * static_cast<float>(width) - 0.5f;
        float y = v * static_cast<float>(height) - 0.5f;

        if (msaaEnabled && msaaSample >= 0 && msaaSample < kMsaaSamples) {
            const Vec2& offset = Framebuffer::msaaOffsets()[msaaSample];
            x += offset.x;
            y += offset.y;
        } else {
            x += 0.5f;
            y += 0.5f;
        }

        const int ix = static_cast<int>(std::floor(x));
        const int iy = static_cast<int>(std::floor(y));
        if (!inBounds(ix, iy)) {
            return std::numeric_limits<float>::infinity();
        }

        const size_t base = static_cast<size_t>(iy * width + ix) * kMsaaSamples;
        if (msaaEnabled && msaaSample >= 0 && msaaSample < kMsaaSamples) {
            if (sampleValid[base + msaaSample]) {
                return sampleDepth[base + msaaSample];
            }
            // 当前子采样无深度时回退到该像素所有有效采样中的最近深度
            float best = std::numeric_limits<float>::infinity();
            for (int s = 0; s < kMsaaSamples; ++s) {
                if (sampleValid[base + s] && sampleDepth[base + s] < best) {
                    best = sampleDepth[base + s];
                }
            }
            return best;
        }

        float best = std::numeric_limits<float>::infinity();
        for (int s = 0; s < kMsaaSamples; ++s) {
            if (sampleValid[base + s] && sampleDepth[base + s] < best) {
                best = sampleDepth[base + s];
            }
        }
        return best;
    }
};
