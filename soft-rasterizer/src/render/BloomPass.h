#pragma once

#include "core/Framebuffer.h"
#include "math/Vec3.h"

#include <cstdint>
#include <limits>
#include <vector>

// 提取自发光区域 → 可分离模糊 → 叠加到主帧缓冲
struct BloomPass {
    int width = 0;
    int height = 0;

    std::vector<Vec3> source;
    std::vector<Vec3> temp;
    std::vector<float> depth;

    float sourceIntensity = 2.8f;
    float whiteEdgeIntensity = 2.4f;
    float whiteThreshold = 0.96f;
    float compositeIntensity = 0.85f;
    int blurRadius = 6;
    int blurPasses = 2;

    void resize(int w, int h);
    void clear();

    bool inBounds(int x, int y) const;
    void putSample(int x, int y, const Vec3& color, float z);

    // 从已解析帧缓冲中提取近白区域的轮廓，叠加到 source
    void accumulateWhiteEdges(const Framebuffer& fb);

    void processAndComposite(Framebuffer& fb) const;

private:
    std::vector<uint8_t> whiteMask;

    void boxBlurHorizontal(const std::vector<Vec3>& src, std::vector<Vec3>& dst, int radius) const;
    void boxBlurVertical(const std::vector<Vec3>& src, std::vector<Vec3>& dst, int radius) const;
};
