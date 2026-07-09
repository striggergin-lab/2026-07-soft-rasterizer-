#include "Texture.h"

#include "Interp.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <vector>

Texture Texture::createCheckerboard(int size, int cells) {
    Texture tex;
    tex.width = size;
    tex.height = size;
    tex.pixels.resize(static_cast<size_t>(size * size));

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool odd = ((x * cells / size) + (y * cells / size)) % 2 == 1;
            tex.pixels[static_cast<size_t>(y * size + x)] =
                odd ? Vec3{0.85f, 0.85f, 0.9f} : Vec3{0.25f, 0.35f, 0.55f};
        }
    }
    return tex;
}

Texture Texture::createFlatNormal() {
    Texture tex;
    tex.width = 1;
    tex.height = 1;
    tex.pixels = {Vec3{0.5f, 0.5f, 1.f}};
    return tex;
}

namespace {

void downscaleRgb(unsigned char* src, int srcW, int srcH, int maxDim, std::vector<unsigned char>& dst,
                  int& dstW, int& dstH) {
    if (srcW <= maxDim && srcH <= maxDim) {
        dstW = srcW;
        dstH = srcH;
        dst.assign(src, src + static_cast<size_t>(srcW * srcH * 3));
        return;
    }

    const float scale = static_cast<float>(maxDim) / static_cast<float>(std::max(srcW, srcH));
    dstW = std::max(1, static_cast<int>(std::lround(srcW * scale)));
    dstH = std::max(1, static_cast<int>(std::lround(srcH * scale)));
    dst.assign(static_cast<size_t>(dstW * dstH * 3), 0);

    const float xRatio = static_cast<float>(srcW) / static_cast<float>(dstW);
    const float yRatio = static_cast<float>(srcH) / static_cast<float>(dstH);

    for (int y = 0; y < dstH; ++y) {
        const int sy0 = std::min(srcH - 1, static_cast<int>(y * yRatio));
        const int sy1 = std::min(srcH - 1, static_cast<int>((y + 1) * yRatio));
        for (int x = 0; x < dstW; ++x) {
            const int sx0 = std::min(srcW - 1, static_cast<int>(x * xRatio));
            const int sx1 = std::min(srcW - 1, static_cast<int>((x + 1) * xRatio));

            int r = 0, g = 0, b = 0, count = 0;
            for (int sy = sy0; sy <= sy1; ++sy) {
                for (int sx = sx0; sx <= sx1; ++sx) {
                    const size_t idx = static_cast<size_t>((sy * srcW + sx) * 3);
                    r += src[idx + 0];
                    g += src[idx + 1];
                    b += src[idx + 2];
                    ++count;
                }
            }
            const size_t out = static_cast<size_t>((y * dstW + x) * 3);
            dst[out + 0] = static_cast<unsigned char>(r / count);
            dst[out + 1] = static_cast<unsigned char>(g / count);
            dst[out + 2] = static_cast<unsigned char>(b / count);
        }
    }
}

}  // namespace

bool Texture::loadFromFile(const std::string& path, int maxDimension) {
    stbi_set_flip_vertically_on_load(1);

    int srcW = 0;
    int srcH = 0;
    int comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &srcW, &srcH, &comp, 3);
    if (!data) {
        width = height = 0;
        pixels.clear();
        return false;
    }

    std::vector<unsigned char> scaled;
    int outW = 0;
    int outH = 0;
    downscaleRgb(data, srcW, srcH, maxDimension, scaled, outW, outH);
    stbi_image_free(data);

    width = outW;
    height = outH;
    pixels.resize(static_cast<size_t>(width * height));
    for (int i = 0; i < width * height; ++i) {
        const size_t idx = static_cast<size_t>(i);
        pixels[idx] = {
            scaled[i * 3 + 0] / 255.f,
            scaled[i * 3 + 1] / 255.f,
            scaled[i * 3 + 2] / 255.f,
        };
    }
    return true;
}

Vec3 Texture::at(int x, int y) const {
    if (!valid()) return {};
    x = x < 0 ? 0 : (x >= width ? width - 1 : x);
    y = y < 0 ? 0 : (y >= height ? height - 1 : y);
    return pixels[static_cast<size_t>(y * width + x)];
}

Vec3 Texture::sampleBilinear(float u, float v) const {
    if (!valid()) return {1.f, 1.f, 1.f};

    u = u - std::floor(u);
    v = v - std::floor(v);

    const float x = u * static_cast<float>(width) - 0.5f;
    const float y = v * static_cast<float>(height) - 0.5f;

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    return bilinearLerp(at(x0, y0), at(x1, y0), at(x0, y1), at(x1, y1), fx, fy);
}

Vec3 Texture::sampleNormalMap(float u, float v, bool flipY) const {
    const Vec3 rgb = sampleBilinear(u, v);
    Vec3 n{
        rgb.x * 2.f - 1.f,
        (rgb.y * 2.f - 1.f) * (flipY ? -1.f : 1.f),
        rgb.z * 2.f - 1.f,
    };
    return n.normalized();
}
