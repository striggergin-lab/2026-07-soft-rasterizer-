#include "Mesh.h"

#include <cmath>

Mesh Mesh::createCube(float size) {
    const float h = size * 0.5f;
    Mesh cube;

    auto addFace = [&](Vec3 n, Vec3 t, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec2 uv0, Vec2 uv1,
                       Vec2 uv2, Vec2 uv3) {
        const unsigned int base = static_cast<unsigned int>(cube.vertices.size());
        const Vec3 white{1.f, 1.f, 1.f};
        cube.vertices.push_back({p0, n, t, white, uv0});
        cube.vertices.push_back({p1, n, t, white, uv1});
        cube.vertices.push_back({p2, n, t, white, uv2});
        cube.vertices.push_back({p3, n, t, white, uv3});
        cube.indices.insert(cube.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };

    // 切线 = UV 的 U 方向在世界空间中的方向
    addFace({0, 0, 1}, {1, 0, 0}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}, {0, 0},
            {1, 0}, {1, 1}, {0, 1});
    addFace({0, 0, -1}, {-1, 0, 0}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}, {0, 0},
            {1, 0}, {1, 1}, {0, 1});
    addFace({1, 0, 0}, {0, 0, -1}, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}, {0, 0}, {1, 0},
            {1, 1}, {0, 1});
    addFace({-1, 0, 0}, {0, 0, 1}, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}, {0, 0},
            {1, 0}, {1, 1}, {0, 1});
    addFace({0, 1, 0}, {1, 0, 0}, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}, {0, 0}, {1, 0},
            {1, 1}, {0, 1});
    addFace({0, -1, 0}, {1, 0, 0}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}, {0, 0},
            {1, 0}, {1, 1}, {0, 1});

    return cube;
}

Mesh Mesh::createFloor(float size) {
    Mesh floor;
    const float h = size * 0.5f;
    const unsigned int base = 0;
    const Vec3 white{1.f, 1.f, 1.f};
    const Vec3 n{0.f, 1.f, 0.f};
    const Vec3 t{1.f, 0.f, 0.f};

    floor.vertices = {
        {{-h, 0.f, -h}, n, t, white, {0, 0}},
        {{-h, 0.f, h}, n, t, white, {0, 1}},
        {{h, 0.f, h}, n, t, white, {1, 1}},
        {{h, 0.f, -h}, n, t, white, {1, 0}},
    };
    floor.indices = {base, base + 1, base + 2, base, base + 2, base + 3};
    return floor;
}

Mesh Mesh::createSphere(float radius, int slices, int stacks) {
    Mesh sphere;

    for (int i = 0; i <= stacks; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(stacks);
        const float phi = v * 3.14159265f;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (int j = 0; j <= slices; ++j) {
            const float u = static_cast<float>(j) / static_cast<float>(slices);
            const float theta = u * 2.f * 3.14159265f;
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            const Vec3 n{sinPhi * cosTheta, cosPhi, sinPhi * sinTheta};
            const Vec3 pos = n * radius;
            const Vec3 tangent{-sinTheta, 0.f, cosTheta};
            const Vec3 color{1.f, 0.95f, 0.7f};
            sphere.vertices.push_back({pos, n, tangent, color, {u, v}});
        }
    }

    const int ring = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const unsigned int a = static_cast<unsigned int>(i * ring + j);
            const unsigned int b = a + static_cast<unsigned int>(ring);
            const unsigned int c = b + 1;
            const unsigned int d = a + 1;
            sphere.indices.insert(sphere.indices.end(), {a, c, b, a, d, c});
        }
    }

    return sphere;
}

namespace {

void addWallQuad(Mesh& mesh, Mesh::WallFacing facing, float y0, float y1, float h0, float h1) {
    const Vec3 white{1.f, 1.f, 1.f};
    const unsigned int base = static_cast<unsigned int>(mesh.vertices.size());

    if (facing == Mesh::WallFacing::PosX) {
        const Vec3 n{1.f, 0.f, 0.f};
        const Vec3 t{0.f, 0.f, -1.f};
        // 与 createCube +X 面一致的绕序，法线朝 +X
        mesh.vertices.push_back({{0.f, y0, h1}, n, t, white, {0.f, 0.f}});
        mesh.vertices.push_back({{0.f, y0, h0}, n, t, white, {1.f, 0.f}});
        mesh.vertices.push_back({{0.f, y1, h0}, n, t, white, {1.f, 1.f}});
        mesh.vertices.push_back({{0.f, y1, h1}, n, t, white, {0.f, 1.f}});
    } else {
        const Vec3 n{0.f, 0.f, 1.f};
        const Vec3 t{1.f, 0.f, 0.f};
        mesh.vertices.push_back({{h0, y0, 0.f}, n, t, white, {0.f, 0.f}});
        mesh.vertices.push_back({{h1, y0, 0.f}, n, t, white, {1.f, 0.f}});
        mesh.vertices.push_back({{h1, y1, 0.f}, n, t, white, {1.f, 1.f}});
        mesh.vertices.push_back({{h0, y1, 0.f}, n, t, white, {0.f, 1.f}});
    }

    mesh.indices.insert(mesh.indices.end(),
                        {base, base + 1, base + 2, base, base + 2, base + 3});
}

}  // namespace

Mesh Mesh::createWall(float width, float height, WallFacing facing) {
    Mesh wall;
    addWallQuad(wall, facing, 0.f, height, 0.f, width);
    return wall;
}

Mesh Mesh::createWallWithWindow(float width, float height, float winW, float winH,
                                WallFacing facing) {
    Mesh wall;
    const float cx = width * 0.5f;
    const float winH0 = cx - winW * 0.5f;
    const float winH1 = cx + winW * 0.5f;
    const float winY0 = 0.f;
    const float winY1 = winH;

    addWallQuad(wall, facing, winY1, height, 0.f, width);
    addWallQuad(wall, facing, winY0, winY1, 0.f, winH0);
    addWallQuad(wall, facing, winY0, winY1, winH1, width);
    return wall;
}
