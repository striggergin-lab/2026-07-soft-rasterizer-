#include "core/Camera.h"
#include "core/Mesh.h"
#include "core/SceneObject.h"
#include "math/Mat4.h"
#include "render/Rasterizer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr float kFloorSize = 10.f;
constexpr float kCubeBaseY = 0.68f;
constexpr uint32_t kBackgroundGray = 0xFF888890;

struct AppState {
    Camera camera;
    Rasterizer rasterizer;
    Mesh floor;
    Mesh cube;
    Mesh lightSphere;
    std::vector<SceneObject> scene;
    bool running = true;
    bool wireframe = false;
    bool cameraOrbiting = false;
    bool cameraPanning = false;
    bool modelRotating = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    float angle = 0.f;
    Mat4 modelRot = Mat4::identity();
    float modelRotateSpeed = 0.012f;
    int width = 960;
    int height = 720;
    HDC memDC = nullptr;
    HBITMAP dib = nullptr;
    void* dibBits = nullptr;
};

AppState* gApp = nullptr;

void updateWindowTitle(HWND hwnd, const AppState& app) {
    wchar_t title[384]{};
    const wchar_t* shadowState = app.rasterizer.shadowSceneEnabled ? L"开" : L"关";
    if (app.rasterizer.materials.empty()) {
        swprintf(title, 384,
                 L"Soft Rasterizer | 阴影:%s | 中键旋转相机 Shift+中键平移 右键旋转物体 | 滚轮缩放",
                 shadowState);
    } else {
        swprintf(title, 384,
                 L"Soft Rasterizer | 材质:%hs (%d/%zu) | 阴影:%s | 右键旋转物体 WASD/QE移光源",
                 app.rasterizer.materials.currentName().c_str(),
                 app.rasterizer.materials.currentIndex + 1, app.rasterizer.materials.count(),
                 shadowState);
    }
    SetWindowTextW(hwnd, title);
}

void rebuildScene(AppState& app) {
    app.scene.clear();

    SceneObject floorObj;
    floorObj.mesh = &app.floor;
    floorObj.model = Mat4::identity();
    floorObj.material = ObjectMaterial::Flat;
    floorObj.flatColor = {1.f, 1.f, 1.f};
    floorObj.castShadow = true;
    floorObj.receiveShadow = true;
    app.scene.push_back(floorObj);

    SceneObject cubeObj;
    cubeObj.mesh = &app.cube;
    cubeObj.model =
        Mat4::translate({0.f, kCubeBaseY, 0.f}) * Mat4::rotateY(app.angle) * app.modelRot;
    cubeObj.material = ObjectMaterial::Textured;
    cubeObj.castShadow = true;
    cubeObj.receiveShadow = true;
    app.scene.push_back(cubeObj);

    if (app.rasterizer.shadowSceneEnabled) {
        SceneObject lightObj;
        lightObj.mesh = &app.lightSphere;
        lightObj.model = Mat4::translate(app.rasterizer.pointLight.position);
        lightObj.material = ObjectMaterial::Emissive;
        lightObj.emissiveColor = {1.f, 0.95f, 0.75f};
        lightObj.castShadow = false;
        lightObj.receiveShadow = false;
        app.scene.push_back(lightObj);
    }
}

bool createBackBuffer(AppState& app, HDC hdc) {
    if (app.memDC) {
        if (app.dib) DeleteObject(app.dib);
        DeleteDC(app.memDC);
        app.memDC = nullptr;
        app.dib = nullptr;
        app.dibBits = nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = app.width;
    bmi.bmiHeader.biHeight = -app.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    app.memDC = CreateCompatibleDC(hdc);
    if (!app.memDC) return false;

    app.dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &app.dibBits, nullptr, 0);
    if (!app.dib || !app.dibBits) {
        if (app.dib) DeleteObject(app.dib);
        DeleteDC(app.memDC);
        app.memDC = nullptr;
        app.dib = nullptr;
        app.dibBits = nullptr;
        return false;
    }

    SelectObject(app.memDC, app.dib);
    app.rasterizer.fb.resize(app.width, app.height);
    return true;
}

void present(HDC windowDC, AppState& app) {
    if (!app.dibBits || !app.memDC) return;
    memcpy(app.dibBits, app.rasterizer.fb.color.data(),
           static_cast<size_t>(app.width) * app.height * sizeof(uint32_t));
    BitBlt(windowDC, 0, 0, app.width, app.height, app.memDC, 0, 0, SRCCOPY);
}

void syncClientSize(AppState& app, HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    if (w == app.width && h == app.height) return;

    app.width = w;
    app.height = h;
    HDC hdc = GetDC(hwnd);
    if (hdc) {
        createBackBuffer(app, hdc);
        ReleaseDC(hwnd, hdc);
    }
}

void updateFrame(AppState& app, float dt) {
    if (app.rasterizer.shadowSceneEnabled) {
        const float ls = app.rasterizer.lightMoveSpeed * dt;
        if (GetAsyncKeyState('W') & 0x8000) app.rasterizer.moveLight(0.f, ls, 0.f);
        if (GetAsyncKeyState('S') & 0x8000) app.rasterizer.moveLight(0.f, -ls, 0.f);
        if (GetAsyncKeyState('A') & 0x8000) app.rasterizer.moveLight(-ls, 0.f, 0.f);
        if (GetAsyncKeyState('D') & 0x8000) app.rasterizer.moveLight(ls, 0.f, 0.f);
        if (GetAsyncKeyState('Q') & 0x8000) app.rasterizer.moveLight(0.f, 0.f, -ls);
        if (GetAsyncKeyState('E') & 0x8000) app.rasterizer.moveLight(0.f, 0.f, ls);
    }

    if (!app.cameraOrbiting && !app.cameraPanning && !app.modelRotating) {
        app.angle += dt * 0.8f;
    }

    app.rasterizer.model = Mat4::rotateY(app.angle);
    app.rasterizer.mode = app.wireframe ? RenderMode::Wireframe : RenderMode::Solid;

    rebuildScene(app);
    app.rasterizer.fb.clear(kBackgroundGray);
    const float aspect =
        static_cast<float>(app.width) / static_cast<float>(app.height > 0 ? app.height : 1);
    app.rasterizer.renderScene(app.scene, app.camera, aspect);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState& app = *gApp;

    switch (msg) {
        case WM_CREATE: {
            HDC hdc = GetDC(hwnd);
            const bool ok = hdc && createBackBuffer(app, hdc);
            ReleaseDC(hwnd, hdc);
            if (!ok) return -1;
            app.rasterizer.loadMaterials();
            updateWindowTitle(hwnd, app);
            SetTimer(hwnd, 1, 16, nullptr);
            return 0;
        }

        case WM_SIZE:
            syncClientSize(app, hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_TIMER: {
            static auto last = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - last).count();
            last = now;

            updateFrame(app, dt);
            HDC hdc = GetDC(hwnd);
            present(hdc, app);
            ReleaseDC(hwnd, hdc);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) app.running = false;
            if (wParam == 'F') app.wireframe = !app.wireframe;
            if (wParam == 'M') app.rasterizer.fb.msaaEnabled = !app.rasterizer.fb.msaaEnabled;
            if (wParam == 'N') app.rasterizer.normalMappingEnabled = !app.rasterizer.normalMappingEnabled;
            if (wParam == 'L') {
                app.rasterizer.shadowSceneEnabled = !app.rasterizer.shadowSceneEnabled;
                updateWindowTitle(hwnd, app);
            }
            if (wParam == VK_UP) {
                app.rasterizer.prevMaterial();
                updateWindowTitle(hwnd, app);
            }
            if (wParam == VK_DOWN) {
                app.rasterizer.nextMaterial();
                updateWindowTitle(hwnd, app);
            }
            return 0;

        case WM_MBUTTONDOWN:
            app.lastMouseX = GET_X_LPARAM(lParam);
            app.lastMouseY = GET_Y_LPARAM(lParam);
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                app.cameraPanning = true;
            } else {
                app.cameraOrbiting = true;
            }
            SetCapture(hwnd);
            return 0;

        case WM_MBUTTONUP:
            app.cameraOrbiting = false;
            app.cameraPanning = false;
            ReleaseCapture();
            return 0;

        case WM_RBUTTONDOWN:
            app.lastMouseX = GET_X_LPARAM(lParam);
            app.lastMouseY = GET_Y_LPARAM(lParam);
            app.modelRotating = true;
            SetCapture(hwnd);
            return 0;

        case WM_RBUTTONUP:
            app.modelRotating = false;
            ReleaseCapture();
            return 0;

        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const float dx = static_cast<float>(x - app.lastMouseX);
            const float dy = static_cast<float>(y - app.lastMouseY);

            if (app.cameraPanning) {
                app.camera.pan(dx, dy);
            } else if (app.cameraOrbiting) {
                app.camera.orbit(dx, dy);
            } else if (app.modelRotating) {
                app.modelRot =
                    Mat4::rotateY(dx * app.modelRotateSpeed) * Mat4::rotateX(-dy * app.modelRotateSpeed) *
                    app.modelRot;
            }
            if (app.cameraOrbiting || app.cameraPanning || app.modelRotating) {
                app.lastMouseX = x;
                app.lastMouseY = y;
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam));
            app.camera.scrollZoom(delta);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    AppState app;
    app.floor = Mesh::createFloor(kFloorSize);
    app.cube = Mesh::createCube(0.8f);
    app.lightSphere = Mesh::createSphere(0.12f, 14, 10);

    // 等轴测：相机在立方体中心斜上方 (1,1,1) 方向，明确朝向中心
    const Vec3 kObjectCenter{0.f, kCubeBaseY, 0.f};
    const Vec3 kCameraPos{2.5f, 3.2f, 2.5f};
    app.camera.setView(kObjectCenter, kCameraPos);

    app.rasterizer.lightTarget = {0.f, kCubeBaseY, 0.f};
    app.rasterizer.pointLight.position = {3.f, 4.f, 3.f};

    gApp = &app;

    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SoftRasterizerWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClass(&wc);

    const int winW = app.width + GetSystemMetrics(SM_CXFIXEDFRAME) * 2;
    const int winH = app.height + GetSystemMetrics(SM_CYFIXEDFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION);

    HWND hwnd = CreateWindow(L"SoftRasterizerWnd", L"Soft Rasterizer",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                             CW_USEDEFAULT, winW, winH, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    syncClientSize(app, hwnd);

    MSG msg{};
    while (app.running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) app.running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (app.memDC) {
        if (app.dib) DeleteObject(app.dib);
        DeleteDC(app.memDC);
    }

    gApp = nullptr;
    return 0;
}
