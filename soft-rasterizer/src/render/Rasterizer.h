#pragma once

#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "core/Mesh.h"
#include "core/SceneObject.h"
#include "math/Mat4.h"
#include "math/Vec2.h"
#include "math/Vec4.h"
#include "render/Lighting.h"
#include "render/MaterialLibrary.h"
#include "render/ShadowMap.h"
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
    ShadowMap shadowMap;
    RenderMode mode = RenderMode::Solid;
    DirectionalLight light{};
    PointLight pointLight{};

    MaterialLibrary materials;
    Texture albedoMap;
    Texture normalMap;
    bool normalMappingEnabled = true;
    bool normalMapFlipY = false;

    bool shadowSceneEnabled = true;
    Vec3 lightTarget{0.f, 0.f, 0.f};
    float lightMoveSpeed = 4.f;
    float shadowBias = 0.02f;

    Mat4 model = Mat4::identity();
    Mat4 lightViewProj = Mat4::identity();

    Rasterizer();

    bool loadMaterials();
    void nextMaterial();
    void prevMaterial();
    void moveLight(float dx, float dy, float dz);

    void renderScene(const std::vector<SceneObject>& objects, const Camera& camera, float aspect);

private:
    void applyCurrentMaterial();

    void renderMeshShadow(const SceneObject& obj, const Mat4& lightViewProj);
    void renderMeshMain(const SceneObject& obj, const Mat4& viewProj, const Vec3& cameraPos);

    std::optional<ShadedFragment> shadeAt(float px, float py, const Vec2& sa, const Vec2& sb,
                                          const Vec2& sc, const ClipVertex& a, const ClipVertex& b,
                                          const ClipVertex& c, float areaInv, const SceneObject& obj,
                                          int msaaSample, const Vec3& cameraPos) const;

    std::optional<float> shadowDepthAt(float px, float py, const Vec2& sa, const Vec2& sb,
                                       const Vec2& sc, const ClipVertex& a, const ClipVertex& b,
                                       const ClipVertex& c, float areaInv) const;

    void drawTriangleColor(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c,
                           const SceneObject& obj, const Vec3& cameraPos);
    void drawTriangleShadow(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c);

    void buildClipTriangles(const SceneObject& obj, const Mat4& mvp, std::vector<std::array<ClipVertex, 3>>& outTris);

    void drawLine(int x0, int y0, int x1, int y1, uint32_t color, float z);
    void drawPoint(int x, int y, uint32_t color, float z);

    static Vec2 toScreen(const Vec4& clipPos, int width, int height);
    static bool clipTriangle(std::array<ClipVertex, 3> in, std::vector<ClipVertex>& out);
    static ClipVertex lerpClip(const ClipVertex& a, const ClipVertex& b, float t);
    static bool insideCVV(const Vec4& p);
    static ClipVertex clipAgainstPlane(const ClipVertex& a, const ClipVertex& b, int plane);
};
