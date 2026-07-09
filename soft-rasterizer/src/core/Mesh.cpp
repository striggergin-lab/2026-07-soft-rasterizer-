#include "Mesh.h"

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
