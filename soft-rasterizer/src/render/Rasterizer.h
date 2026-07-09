#pragma once

#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "core/Mesh.h"
#include "math/Mat4.h"
#include "math/Vec2.h"
#include "math/Vec4.h"
#include "render/Lighting.h"
#include "render/MaterialLibrary.h"
#include "render/Texture.h"

#include <array>
#include <optional>
#include <vector>

enum class RenderMode { Solid, Wireframe };

struct ClipVertex {
    Vec4 position;
    Vec3 worldPos;
    Vec3 worldNormal;
    Vec3 worldTangent;
    Vec3 color;
    Vec2 uv;
    float invW = 1.f;
};

struct ShadedFragment {
    Vec3 color;
    float depth = 0.f;
};

struct Rasterizer {
    Framebuffer fb;
    RenderMode mode = RenderMode::Solid;
    DirectionalLight light{};
    Mat4 model = Mat4::identity();

    MaterialLibrary materials;
    Texture albedoMap;
    Texture normalMap;
    bool normalMappingEnabled = true;
    bool normalMapFlipY = false;

    Rasterizer();

    bool loadMaterials();
    void nextMaterial();
    void prevMaterial();
    void render(const Mesh& mesh, const Camera& camera, float aspect);

private:
    void applyCurrentMaterial();

    std::optional<ShadedFragment> shadeAt(float px, float py, const Vec2& sa, const Vec2& sb,
                                          const Vec2& sc, const ClipVertex& a, const ClipVertex& b,
                                          const ClipVertex& c, float areaInv,
                                          const Vec3& cameraPos) const;

    void drawTriangle(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c,
                      const Vec3& cameraPos);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t color, float z);
    void drawPoint(int x, int y, uint32_t color, float z);

    static Vec2 toScreen(const Vec4& clipPos, int width, int height);
    static bool clipTriangle(std::array<ClipVertex, 3> in, std::vector<ClipVertex>& out);
    static ClipVertex lerpClip(const ClipVertex& a, const ClipVertex& b, float t);
    static bool insideCVV(const Vec4& p);
    static ClipVertex clipAgainstPlane(const ClipVertex& a, const ClipVertex& b, int plane);
};
