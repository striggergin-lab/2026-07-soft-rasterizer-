#include "Rasterizer.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace {

constexpr float kPi = 3.14159265358979323846f;

Mat4 normalMatrix(const Mat4& model) {
    // 法线变换：模型矩阵的逆转置（此处仅旋转+均匀缩放时可简化为 model 旋转部分）
    return model;
}

Vec3 transformNormal(const Mat4& m, const Vec3& n) {
    Vec3 r{
        m.m[0][0] * n.x + m.m[0][1] * n.y + m.m[0][2] * n.z,
        m.m[1][0] * n.x + m.m[1][1] * n.y + m.m[1][2] * n.z,
        m.m[2][0] * n.x + m.m[2][1] * n.y + m.m[2][2] * n.z,
    };
    return r.normalized();
}

}  // namespace

Rasterizer::Rasterizer() {
    albedoMap = Texture::createCheckerboard();
    normalMap = Texture::createFlatNormal();
}

bool Rasterizer::loadMaterials() {
    if (!materials.scan("image")) {
        return false;
    }
    applyCurrentMaterial();
    return true;
}

void Rasterizer::applyCurrentMaterial() {
    if (!materials.applyCurrent(albedoMap, normalMap)) {
        albedoMap = Texture::createCheckerboard();
        normalMap = Texture::createFlatNormal();
        normalMappingEnabled = false;
        return;
    }
    normalMappingEnabled = normalMap.valid();
}

void Rasterizer::nextMaterial() {
    if (materials.next()) {
        applyCurrentMaterial();
    }
}

void Rasterizer::prevMaterial() {
    if (materials.prev()) {
        applyCurrentMaterial();
    }
}

void Rasterizer::render(const Mesh& mesh, const Camera& camera, float aspect) {
    // 【重点】MVP = P * V * M，依次：模型→世界→观察→裁剪
    const Mat4 view = camera.viewMatrix();
    const Mat4 proj = Mat4::perspective(60.f * kPi / 180.f, aspect, 0.1f, 100.f);
    const Mat4 mvp = proj * view * model;
    const Mat4 normalMat = normalMatrix(model);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::array<ClipVertex, 3> tri{};
        for (int v = 0; v < 3; ++v) {
            const Vertex& src = mesh.vertices[mesh.indices[i + v]];
            const Vec4 clip = mvp.transformPoint(Vec4(src.position, 1.f));

            tri[v].position = clip;
            tri[v].worldPos = {
                model.m[0][0] * src.position.x + model.m[0][1] * src.position.y +
                    model.m[0][2] * src.position.z + model.m[0][3],
                model.m[1][0] * src.position.x + model.m[1][1] * src.position.y +
                    model.m[1][2] * src.position.z + model.m[1][3],
                model.m[2][0] * src.position.x + model.m[2][1] * src.position.y +
                    model.m[2][2] * src.position.z + model.m[2][3],
            };
            tri[v].worldNormal = transformNormal(normalMat, src.normal);
            tri[v].worldTangent = transformNormal(normalMat, src.tangent);
            tri[v].color = src.color;
            tri[v].uv = src.uv;
            tri[v].invW = clip.w > 1e-6f ? (1.f / clip.w) : 1.f;
        }

        // 裁剪阶段：CVV 外的部分切掉，可能产生 3~4 边形，再三角化
        std::vector<ClipVertex> clipped;
        if (!clipTriangle(tri, clipped)) continue;

        for (size_t t = 0; t + 2 < clipped.size(); t += 3) {
            drawTriangle(clipped[t], clipped[t + 1], clipped[t + 2], camera.position);
        }
    }

    fb.resolveMsaa();
}

Vec2 Rasterizer::toScreen(const Vec4& clipPos, int width, int height) {
    // clip.w 在 OpenGL 透视下等于 -viewZ；必须 w>0 才是相机前方
    if (clipPos.w <= 1e-6f) {
        return {-1e6f, -1e6f};
    }
    const float invW = 1.f / clipPos.w;
    const float ndcX = clipPos.x * invW;
    const float ndcY = clipPos.y * invW;
    return {
        (ndcX * 0.5f + 0.5f) * static_cast<float>(width),
        (1.f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height),
    };
}

bool Rasterizer::insideCVV(const Vec4& p) {
    return p.x >= -p.w && p.x <= p.w && p.y >= -p.w && p.y <= p.w && p.z >= -p.w && p.z <= p.w;
}

ClipVertex Rasterizer::lerpClip(const ClipVertex& a, const ClipVertex& b, float t) {
    auto lerp4 = [](const Vec4& x, const Vec4& y, float u) {
        return Vec4{
            x.x + (y.x - x.x) * u,
            x.y + (y.y - x.y) * u,
            x.z + (y.z - x.z) * u,
            x.w + (y.w - x.w) * u,
        };
    };
    auto lerp3 = [](const Vec3& x, const Vec3& y, float u) { return x + (y - x) * u; };

    ClipVertex o;
    o.position = lerp4(a.position, b.position, t);
    o.worldPos = lerp3(a.worldPos, b.worldPos, t);
    o.worldNormal = lerp3(a.worldNormal, b.worldNormal, t);
    o.worldTangent = lerp3(a.worldTangent, b.worldTangent, t);
    o.color = lerp3(a.color, b.color, t);
    o.uv = a.uv + (b.uv - a.uv) * t;
    o.invW = o.position.w > 1e-6f ? 1.f / o.position.w : 1.f;
    return o;
}

ClipVertex Rasterizer::clipAgainstPlane(const ClipVertex& a, const ClipVertex& b, int plane) {
    const auto dist = [plane](const Vec4& p) {
        switch (plane) {
            case -1: return p.w;  // 相机前方：w > 0
            case 0: return p.w - p.x;
            case 1: return p.w + p.x;
            case 2: return p.w - p.y;
            case 3: return p.w + p.y;
            case 4: return p.w - p.z;
            default: return p.w + p.z;
        }
    };

    const float da = dist(a.position);
    const float db = dist(b.position);
    const float t = da / (da - db + 1e-8f);
    return lerpClip(a, b, t);
}

bool Rasterizer::clipTriangle(std::array<ClipVertex, 3> in, std::vector<ClipVertex>& out) {
    std::vector<ClipVertex> poly(in.begin(), in.end());

    // 先剔除相机后方（clip.w <= 0）
    {
        std::vector<ClipVertex> next;
        const size_t n = poly.size();
        for (size_t i = 0; i < n; ++i) {
            const ClipVertex& cur = poly[i];
            const ClipVertex& prev = poly[(i + n - 1) % n];
            const bool curIn = cur.position.w > 1e-6f;
            const bool prevIn = prev.position.w > 1e-6f;
            if (curIn) {
                if (!prevIn) next.push_back(clipAgainstPlane(prev, cur, -1));
                next.push_back(cur);
            } else if (prevIn) {
                next.push_back(clipAgainstPlane(prev, cur, -1));
            }
        }
        poly.swap(next);
    }

    // 【重点】CVV：clip 空间下 |x|,|y|,|z| <= w 的齐次立方体
    // 透视除法前裁剪，避免 w<0 导致的投影错误
    for (int plane = 0; plane < 6; ++plane) {
        if (poly.empty()) return false;
        std::vector<ClipVertex> next;
        const size_t n = poly.size();
        for (size_t i = 0; i < n; ++i) {
            const ClipVertex& cur = poly[i];
            const ClipVertex& prev = poly[(i + n - 1) % n];
            const auto dist = [plane](const Vec4& p) {
                switch (plane) {
                    case 0: return p.w - p.x;
                    case 1: return p.w + p.x;
                    case 2: return p.w - p.y;
                    case 3: return p.w + p.y;
                    case 4: return p.w - p.z;
                    default: return p.w + p.z;
                }
            };
            const bool curIn = dist(cur.position) >= 0.f;
            const bool prevIn = dist(prev.position) >= 0.f;
            if (curIn) {
                if (!prevIn) next.push_back(clipAgainstPlane(prev, cur, plane));
                next.push_back(cur);
            } else if (prevIn) {
                next.push_back(clipAgainstPlane(prev, cur, plane));
            }
        }
        poly.swap(next);
    }

    if (poly.size() < 3) return false;

    // 凸多边形扇形三角化
    out.clear();
    for (size_t i = 1; i + 1 < poly.size(); ++i) {
        out.push_back(poly[0]);
        out.push_back(poly[i]);
        out.push_back(poly[i + 1]);
    }
    return !out.empty();
}

void Rasterizer::drawLine(int x0, int y0, int x1, int y1, uint32_t color, float z) {
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        fb.putPixel(x0, y0, color, z);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Rasterizer::drawPoint(int x, int y, uint32_t color, float z) {
    fb.putPixel(x, y, color, z);
}

std::optional<ShadedFragment> Rasterizer::shadeAt(float px, float py, const Vec2& sa,
                                                  const Vec2& sb, const Vec2& sc,
                                                  const ClipVertex& a, const ClipVertex& b,
                                                  const ClipVertex& c, float areaInv,
                                                  const Vec3& cameraPos) const {
    const float wA = ((sb.x - px) * (sc.y - py) - (sc.x - px) * (sb.y - py)) * areaInv;
    const float wB = ((sc.x - px) * (sa.y - py) - (sa.x - px) * (sc.y - py)) * areaInv;
    const float wC = 1.f - wA - wB;

    if (wA < 0.f || wB < 0.f || wC < 0.f) return std::nullopt;

    const float invW = wA * a.invW + wB * b.invW + wC * c.invW;
    if (invW <= 1e-8f) return std::nullopt;

    const auto perspLerp3 = [&](const Vec3& va, const Vec3& vb, const Vec3& vc) {
        const Vec3 num = va * (wA * a.invW) + vb * (wB * b.invW) + vc * (wC * c.invW);
        return num / invW;
    };
    const auto perspLerp2 = [&](const Vec2& va, const Vec2& vb, const Vec2& vc) {
        const Vec2 num = va * (wA * a.invW) + vb * (wB * b.invW) + vc * (wC * c.invW);
        return num / invW;
    };

    const Vec3 worldPos = perspLerp3(a.worldPos, b.worldPos, c.worldPos);
    Vec3 worldNormal = perspLerp3(a.worldNormal, b.worldNormal, c.worldNormal);
    const Vec3 worldTangent = perspLerp3(a.worldTangent, b.worldTangent, c.worldTangent);
    const Vec2 uv = perspLerp2(a.uv, b.uv, c.uv);

    const Vec3 albedo = albedoMap.sampleBilinear(uv.x, uv.y);

    if (normalMappingEnabled && normalMap.valid()) {
        const Vec3 nTangent = normalMap.sampleNormalMap(uv.x, uv.y, normalMapFlipY);
        worldNormal = tangentToWorld(nTangent, worldNormal, worldTangent);
    }

    const float depth = 1.f / invW;

    ShadedFragment out;
    out.color = blinnPhong(worldPos, worldNormal, cameraPos, albedo, light);
    out.depth = depth;
    return out;
}

void Rasterizer::drawTriangle(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c,
                              const Vec3& cameraPos) {
    const Vec2 sa = toScreen(a.position, fb.width, fb.height);
    const Vec2 sb = toScreen(b.position, fb.width, fb.height);
    const Vec2 sc = toScreen(c.position, fb.width, fb.height);
    if (sa.x < -1e5f || sb.x < -1e5f || sc.x < -1e5f) return;

    const Vec3 ndcA = a.position.perspectiveDivide();
    const Vec3 ndcB = b.position.perspectiveDivide();
    const Vec3 ndcC = c.position.perspectiveDivide();

    if (mode == RenderMode::Wireframe) {
        const uint32_t col = Framebuffer::packColor({1.f, 1.f, 1.f});
        // 线框用顶点 NDC 深度均值即可
        const float depth = (ndcA.z + ndcB.z + ndcC.z) / 3.f;
        drawLine(static_cast<int>(sa.x), static_cast<int>(sa.y), static_cast<int>(sb.x),
                 static_cast<int>(sb.y), col, depth);
        drawLine(static_cast<int>(sb.x), static_cast<int>(sb.y), static_cast<int>(sc.x),
                 static_cast<int>(sc.y), col, depth);
        drawLine(static_cast<int>(sc.x), static_cast<int>(sc.y), static_cast<int>(sa.x),
                 static_cast<int>(sa.y), col, depth);
        return;
    }

    // 背面剔除：toScreen 做了 Y 翻转（NDC Y↑ → 屏幕 Y↓），
    // 模型空间 CCW 正面在屏幕空间变为 CW（area < 0），应剔除 CCW 背面（area >= 0）
    const float area =
        (sb.x - sa.x) * (sc.y - sa.y) - (sc.x - sa.x) * (sb.y - sa.y);
    if (area >= 0.f) return;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({sa.x, sb.x, sc.x}))));
    const int maxX =
        std::min(fb.width - 1, static_cast<int>(std::ceil(std::max({sa.x, sb.x, sc.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({sa.y, sb.y, sc.y}))));
    const int maxY =
        std::min(fb.height - 1, static_cast<int>(std::ceil(std::max({sa.y, sb.y, sc.y}))));

    const float areaInv = 1.f / area;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (fb.msaaEnabled) {
                for (int s = 0; s < Framebuffer::kMsaaSamples; ++s) {
                    const Vec2& offset = Framebuffer::msaaOffsets()[s];
                    const float px = static_cast<float>(x) + offset.x;
                    const float py = static_cast<float>(y) + offset.y;

                    const auto frag =
                        shadeAt(px, py, sa, sb, sc, a, b, c, areaInv, cameraPos);
                    if (frag) {
                        fb.putSample(x, y, s, frag->color, frag->depth);
                    }
                }
            } else {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                const auto frag =
                    shadeAt(px, py, sa, sb, sc, a, b, c, areaInv, cameraPos);
                if (frag) {
                    fb.putPixel(x, y, Framebuffer::packColor(frag->color), frag->depth);
                }
            }
        }
    }
}
