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
    std::vector<float> sampleStrength;
    std::vector<uint8_t> sampleValid;

    void resize(int w, int h) {
        width = w;
        height = h;
        const size_t count = static_cast<size_t>(w * h * kMsaaSamples);
        sampleDepth.assign(count, std::numeric_limits<float>::infinity());
        sampleStrength.assign(count, 1.f);
        sampleValid.assign(count, 0);
    }

    void clear() {
        std::fill(sampleDepth.begin(), sampleDepth.end(), std::numeric_limits<float>::infinity());
        std::fill(sampleStrength.begin(), sampleStrength.end(), 1.f);
        std::fill(sampleValid.begin(), sampleValid.end(), static_cast<uint8_t>(0));
    }

    bool inBounds(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    void putSample(int x, int y, int sample, float depth, float strength = 1.f) {
        if (!inBounds(x, y) || sample < 0 || sample >= kMsaaSamples) return;
        const size_t idx = static_cast<size_t>(y * width + x) * kMsaaSamples + sample;
        if (depth < sampleDepth[idx]) {
            sampleDepth[idx] = depth;
            sampleStrength[idx] = strength;
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

    float sampleStrengthAt(float u, float v, int msaaSample) const {
        if (u < 0.f || u > 1.f || v < 0.f || v > 1.f) {
            return 1.f;
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
            return 1.f;
        }

        const size_t base = static_cast<size_t>(iy * width + ix) * kMsaaSamples;
        if (msaaEnabled && msaaSample >= 0 && msaaSample < kMsaaSamples) {
            if (sampleValid[base + msaaSample]) {
                return sampleStrength[base + msaaSample];
            }
            float bestDepth = std::numeric_limits<float>::infinity();
            float strength = 1.f;
            for (int s = 0; s < kMsaaSamples; ++s) {
                if (sampleValid[base + s] && sampleDepth[base + s] < bestDepth) {
                    bestDepth = sampleDepth[base + s];
                    strength = sampleStrength[base + s];
                }
            }
            return strength;
        }

        float bestDepth = std::numeric_limits<float>::infinity();
        float strength = 1.f;
        for (int s = 0; s < kMsaaSamples; ++s) {
            if (sampleValid[base + s] && sampleDepth[base + s] < bestDepth) {
                bestDepth = sampleDepth[base + s];
                strength = sampleStrength[base + s];
            }
        }
        return strength;
    }

    // 2×2 PCF：对 Shadow Map 做 4 点采样并平均，减轻阴影边缘摩尔纹
    float computePcfShadowFactor(float u, float v, float fragDist, float bias, int msaaSample,
                                 float litFactor = 1.f, float darkFactor = 0.18f) const {
        if (u < 0.f || u > 1.f || v < 0.f || v > 1.f) {
            return litFactor;
        }

        const float texelU = 1.f / static_cast<float>(width);
        const float texelV = 1.f / static_cast<float>(height);

        float visibility = 0.f;
        float occluderStrength = 0.f;
        for (int oy = 0; oy < 2; ++oy) {
            for (int ox = 0; ox < 2; ++ox) {
                const float su = u + (static_cast<float>(ox) - 0.5f) * texelU;
                const float sv = v + (static_cast<float>(oy) - 0.5f) * texelV;
                const float mapDepth = sampleDepthAt(su, sv, msaaSample);
                occluderStrength += sampleStrengthAt(su, sv, msaaSample);
                if (mapDepth >= std::numeric_limits<float>::infinity() ||
                    fragDist <= mapDepth + bias) {
                    visibility += 1.f;
                }
            }
        }
        visibility *= 0.25f;
        occluderStrength *= 0.25f;
        const float shadowDarkness = (1.f - visibility) * occluderStrength;
        return litFactor - (litFactor - darkFactor) * shadowDarkness;
    }
};
