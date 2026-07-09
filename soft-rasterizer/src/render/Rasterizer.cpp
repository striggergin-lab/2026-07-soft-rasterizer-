#include "Rasterizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace {

constexpr float kPi = 3.14159265358979323846f;

Mat4 normalMatrix(const Mat4& model) { return model; }

Vec3 transformNormal(const Mat4& m, const Vec3& n) {
    Vec3 r{
        m.m[0][0] * n.x + m.m[0][1] * n.y + m.m[0][2] * n.z,
        m.m[1][0] * n.x + m.m[1][1] * n.y + m.m[1][2] * n.z,
        m.m[2][0] * n.x + m.m[2][1] * n.y + m.m[2][2] * n.z,
    };
    return r.normalized();
}

Vec3 transformPoint3(const Mat4& m, const Vec3& p) {
    return {
        m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
        m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
        m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3],
    };
}

}  // namespace

Rasterizer::Rasterizer() {
    albedoMap = Texture::createCheckerboard();
    normalMap = Texture::createFlatNormal();
    shadowMap.resize(1024, 1024);
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

void Rasterizer::moveLight(float dx, float dy, float dz) {
    pointLight.position.x += dx;
    pointLight.position.y += dy;
    pointLight.position.z += dz;
}

void Rasterizer::renderScene(const std::vector<SceneObject>& objects, const Camera& camera,
                             float aspect) {
    shadowMap.msaaEnabled = fb.msaaEnabled;

    if (shadowSceneEnabled) {
        shadowMap.clear();
        const Mat4 lightView = Mat4::lookAt(pointLight.position, lightTarget, Vec3{0.f, 1.f, 0.f});
        const Mat4 lightProj = Mat4::perspective(90.f * kPi / 180.f, 1.f, 0.2f, 35.f);
        lightViewProj = lightProj * lightView;

        for (const SceneObject& obj : objects) {
            if (!obj.castShadow || obj.material == ObjectMaterial::Emissive) continue;
            renderMeshShadow(obj, lightViewProj);
        }
    }

    const Mat4 view = camera.viewMatrix();
    const Mat4 proj = Mat4::perspective(60.f * kPi / 180.f, aspect, 0.1f, 100.f);
    const Mat4 viewProj = proj * view;

    for (const SceneObject& obj : objects) {
        if (shadowSceneEnabled || obj.material != ObjectMaterial::Emissive) {
            renderMeshMain(obj, viewProj, camera.position);
        }
    }

    fb.resolveMsaa();
}

void Rasterizer::buildClipTriangles(const SceneObject& obj, const Mat4& mvp,
                                    std::vector<std::array<ClipVertex, 3>>& outTris) {
    const Mat4 normalMat = normalMatrix(obj.model);

    for (size_t i = 0; i + 2 < obj.mesh->indices.size(); i += 3) {
        std::array<ClipVertex, 3> tri{};
        for (int v = 0; v < 3; ++v) {
            const Vertex& src = obj.mesh->vertices[obj.mesh->indices[i + v]];
            const Vec4 clip = mvp.transformPoint(Vec4(src.position, 1.f));

            tri[v].position = clip;
            tri[v].worldPos = transformPoint3(obj.model, src.position);
            tri[v].worldNormal = transformNormal(normalMat, src.normal);
            tri[v].worldTangent = transformNormal(normalMat, src.tangent);
            tri[v].color = src.color;
            tri[v].uv = src.uv;
            tri[v].invW = clip.w > 1e-6f ? (1.f / clip.w) : 1.f;
        }

        std::vector<ClipVertex> clipped;
        if (!clipTriangle(tri, clipped)) continue;

        for (size_t t = 0; t + 2 < clipped.size(); t += 3) {
            outTris.push_back({clipped[t], clipped[t + 1], clipped[t + 2]});
        }
    }
}

void Rasterizer::renderMeshShadow(const SceneObject& obj, const Mat4& lightVP) {
    const Mat4 mvp = lightVP * obj.model;
    std::vector<std::array<ClipVertex, 3>> tris;
    buildClipTriangles(obj, mvp, tris);

    for (const auto& tri : tris) {
        drawTriangleShadow(tri[0], tri[1], tri[2]);
    }
}

void Rasterizer::renderMeshMain(const SceneObject& obj, const Mat4& viewProj,
                                const Vec3& cameraPos) {
    const Mat4 mvp = viewProj * obj.model;
    std::vector<std::array<ClipVertex, 3>> tris;
    buildClipTriangles(obj, mvp, tris);

    for (const auto& tri : tris) {
        drawTriangleColor(tri[0], tri[1], tri[2], obj, cameraPos);
    }
}

Vec2 Rasterizer::toScreen(const Vec4& clipPos, int width, int height) {
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
            case -1: return p.w;
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

std::optional<float> Rasterizer::shadowDepthAt(float px, float py, const Vec2& sa, const Vec2& sb,
                                               const Vec2& sc, const ClipVertex& a,
                                               const ClipVertex& b, const ClipVertex& c,
                                               float areaInv) const {
    const float wA = ((sb.x - px) * (sc.y - py) - (sc.x - px) * (sb.y - py)) * areaInv;
    const float wB = ((sc.x - px) * (sa.y - py) - (sa.x - px) * (sc.y - py)) * areaInv;
    const float wC = 1.f - wA - wB;

    if (wA < 0.f || wB < 0.f || wC < 0.f) return std::nullopt;

    const float invW = wA * a.invW + wB * b.invW + wC * c.invW;
    if (invW <= 1e-8f) return std::nullopt;

    // 与主 Z-Buffer 一致：透视矫正后的 clip.w，越小越近
    return 1.f / invW;
}

void Rasterizer::drawTriangleShadow(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c) {
    const Vec2 sa = toScreen(a.position, shadowMap.width, shadowMap.height);
    const Vec2 sb = toScreen(b.position, shadowMap.width, shadowMap.height);
    const Vec2 sc = toScreen(c.position, shadowMap.width, shadowMap.height);
    if (sa.x < -1e5f || sb.x < -1e5f || sc.x < -1e5f) return;

    const float area =
        (sb.x - sa.x) * (sc.y - sa.y) - (sc.x - sa.x) * (sb.y - sa.y);
    if (area >= 0.f) return;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({sa.x, sb.x, sc.x}))));
    const int maxX =
        std::min(shadowMap.width - 1, static_cast<int>(std::ceil(std::max({sa.x, sb.x, sc.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({sa.y, sb.y, sc.y}))));
    const int maxY =
        std::min(shadowMap.height - 1, static_cast<int>(std::ceil(std::max({sa.y, sb.y, sc.y}))));

    const float areaInv = 1.f / area;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (shadowMap.msaaEnabled) {
                for (int s = 0; s < ShadowMap::kMsaaSamples; ++s) {
                    const Vec2& offset = Framebuffer::msaaOffsets()[s];
                    const float px = static_cast<float>(x) + offset.x;
                    const float py = static_cast<float>(y) + offset.y;

                    const auto depth =
                        shadowDepthAt(px, py, sa, sb, sc, a, b, c, areaInv);
                    if (depth) {
                        shadowMap.putSample(x, y, s, *depth);
                    }
                }
            } else {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                const auto depth =
                    shadowDepthAt(px, py, sa, sb, sc, a, b, c, areaInv);
                if (depth) {
                    shadowMap.putSample(x, y, 0, *depth);
                }
            }
        }
    }
}

std::optional<ShadedFragment> Rasterizer::shadeAt(float px, float py, const Vec2& sa,
                                                    const Vec2& sb, const Vec2& sc,
                                                    const ClipVertex& a, const ClipVertex& b,
                                                    const ClipVertex& c, float areaInv,
                                                    const SceneObject& obj, int msaaSample,
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

    ShadedFragment out;
    out.depth = 1.f / invW;

    if (obj.material == ObjectMaterial::Emissive) {
        out.color = obj.emissiveColor;
        return out;
    }

    Vec3 albedo = obj.flatColor;
    if (obj.material == ObjectMaterial::Textured) {
        albedo = albedoMap.sampleBilinear(uv.x, uv.y);
        if (normalMappingEnabled && normalMap.valid()) {
            const Vec3 nTangent = normalMap.sampleNormalMap(uv.x, uv.y, normalMapFlipY);
            worldNormal = tangentToWorld(nTangent, worldNormal, worldTangent);
        }
    }

    if (shadowSceneEnabled) {
        float shadowFactor = 1.f;
        if (obj.receiveShadow) {
            const Vec4 lc = lightViewProj.transformPoint(Vec4(worldPos, 1.f));
            if (lc.w > 1e-6f) {
                const float lcInvW = 1.f / lc.w;
                const float u = lc.x * lcInvW * 0.5f + 0.5f;
                const float v = 1.f - (lc.y * lcInvW * 0.5f + 0.5f);
                const float fragDepth = lc.w;
                const float mapDepth = shadowMap.sampleDepthAt(u, v, msaaSample);
                if (mapDepth < std::numeric_limits<float>::infinity() &&
                    fragDepth > mapDepth + shadowBias) {
                    shadowFactor = 0.18f;
                }
            }
        }
        out.color = blinnPhongPoint(worldPos, worldNormal, cameraPos, albedo, pointLight, shadowFactor);
    } else {
        out.color = blinnPhong(worldPos, worldNormal, cameraPos, albedo, light);
    }

    return out;
}

void Rasterizer::drawTriangleColor(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c,
                                   const SceneObject& obj, const Vec3& cameraPos) {
    const Vec2 sa = toScreen(a.position, fb.width, fb.height);
    const Vec2 sb = toScreen(b.position, fb.width, fb.height);
    const Vec2 sc = toScreen(c.position, fb.width, fb.height);
    if (sa.x < -1e5f || sb.x < -1e5f || sc.x < -1e5f) return;

    const Vec3 ndcA = a.position.perspectiveDivide();
    const Vec3 ndcB = b.position.perspectiveDivide();
    const Vec3 ndcC = c.position.perspectiveDivide();

    if (mode == RenderMode::Wireframe) {
        const uint32_t col = Framebuffer::packColor({1.f, 1.f, 1.f});
        const float depth = (ndcA.z + ndcB.z + ndcC.z) / 3.f;
        drawLine(static_cast<int>(sa.x), static_cast<int>(sa.y), static_cast<int>(sb.x),
                 static_cast<int>(sb.y), col, depth);
        drawLine(static_cast<int>(sb.x), static_cast<int>(sb.y), static_cast<int>(sc.x),
                 static_cast<int>(sc.y), col, depth);
        drawLine(static_cast<int>(sc.x), static_cast<int>(sc.y), static_cast<int>(sa.x),
                 static_cast<int>(sa.y), col, depth);
        return;
    }

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
                        shadeAt(px, py, sa, sb, sc, a, b, c, areaInv, obj, s, cameraPos);
                    if (frag) {
                        fb.putSample(x, y, s, frag->color, frag->depth);
                    }
                }
            } else {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                const auto frag =
                    shadeAt(px, py, sa, sb, sc, a, b, c, areaInv, obj, -1, cameraPos);
                if (frag) {
                    fb.putPixel(x, y, Framebuffer::packColor(frag->color), frag->depth);
                }
            }
        }
    }
}
