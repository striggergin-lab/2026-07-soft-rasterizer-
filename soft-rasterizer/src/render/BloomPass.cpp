#include "BloomPass.h"

#include <algorithm>
#include <cmath>

void BloomPass::resize(int w, int h) {
    width = w;
    height = h;
    const size_t count = static_cast<size_t>(w * h);
    source.assign(count, Vec3{0.f, 0.f, 0.f});
    temp.assign(count, Vec3{0.f, 0.f, 0.f});
    depth.assign(count, std::numeric_limits<float>::infinity());
    whiteMask.assign(count, 0);
}

void BloomPass::clear() {
    std::fill(source.begin(), source.end(), Vec3{0.f, 0.f, 0.f});
    std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::infinity());
}

namespace {

bool isNearWhite(const Vec3& color, float threshold) {
    return color.x >= threshold && color.y >= threshold && color.z >= threshold;
}

}  // namespace

void BloomPass::accumulateWhiteEdges(const Framebuffer& fb) {
    if (width <= 0 || height <= 0 ||
        static_cast<int>(source.size()) != width * height ||
        static_cast<int>(fb.color.size()) != width * height) {
        return;
    }

    const size_t count = static_cast<size_t>(width * height);
    for (size_t idx = 0; idx < count; ++idx) {
        const Vec3 color = Framebuffer::unpackColor(fb.color[idx]);
        whiteMask[idx] = isNearWhite(color, whiteThreshold) ? 1 : 0;
    }

    auto maskAt = [&](int x, int y) -> bool {
        if (!inBounds(x, y)) return false;
        return whiteMask[static_cast<size_t>(y * width + x)] != 0;
    };

    static constexpr int kNeighborDx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    static constexpr int kNeighborDy[] = {0, 0, -1, 1, -1, 1, -1, 1};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = static_cast<size_t>(y * width + x);
            if (!whiteMask[idx]) continue;

            bool edge = false;
            for (int n = 0; n < 8; ++n) {
                if (!maskAt(x + kNeighborDx[n], y + kNeighborDy[n])) {
                    edge = true;
                    break;
                }
            }
            if (!edge) continue;

            const Vec3 color = Framebuffer::unpackColor(fb.color[idx]);
            source[idx] = source[idx] + color * whiteEdgeIntensity;
        }
    }
}

bool BloomPass::inBounds(int x, int y) const {
    return x >= 0 && x < width && y >= 0 && y < height;
}

void BloomPass::putSample(int x, int y, const Vec3& color, float z) {
    if (!inBounds(x, y)) return;
    const size_t idx = static_cast<size_t>(y * width + x);
    if (z < depth[idx]) {
        depth[idx] = z;
        source[idx] = color;
    }
}

void BloomPass::boxBlurHorizontal(const std::vector<Vec3>& src, std::vector<Vec3>& dst,
                                  int radius) const {
    const int r = std::max(1, radius);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Vec3 accum{};
            int count = 0;
            const int x0 = std::max(0, x - r);
            const int x1 = std::min(width - 1, x + r);
            for (int sx = x0; sx <= x1; ++sx) {
                accum = accum + src[static_cast<size_t>(y * width + sx)];
                ++count;
            }
            dst[static_cast<size_t>(y * width + x)] = accum / static_cast<float>(count);
        }
    }
}

void BloomPass::boxBlurVertical(const std::vector<Vec3>& src, std::vector<Vec3>& dst,
                                int radius) const {
    const int r = std::max(1, radius);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Vec3 accum{};
            int count = 0;
            const int y0 = std::max(0, y - r);
            const int y1 = std::min(height - 1, y + r);
            for (int sy = y0; sy <= y1; ++sy) {
                accum = accum + src[static_cast<size_t>(sy * width + x)];
                ++count;
            }
            dst[static_cast<size_t>(y * width + x)] = accum / static_cast<float>(count);
        }
    }
}

void BloomPass::processAndComposite(Framebuffer& fb) const {
    if (width <= 0 || height <= 0 || static_cast<int>(source.size()) != width * height) {
        return;
    }

    std::vector<Vec3> blurred = source;
    std::vector<Vec3> ping = source;
    std::vector<Vec3> pong = temp;

    for (int pass = 0; pass < blurPasses; ++pass) {
        boxBlurHorizontal(blurred, ping, blurRadius);
        boxBlurVertical(ping, pong, blurRadius);
        blurred.swap(pong);
    }

    const size_t count = static_cast<size_t>(width * height);
    for (size_t idx = 0; idx < count; ++idx) {
        const Vec3 scene = Framebuffer::unpackColor(fb.color[idx]);
        Vec3 result = scene + blurred[idx] * compositeIntensity;
        result.x = std::min(result.x, 1.f);
        result.y = std::min(result.y, 1.f);
        result.z = std::min(result.z, 1.f);
        fb.color[idx] = Framebuffer::packColor(result);
    }
}
