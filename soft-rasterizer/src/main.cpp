#include "core/Camera.h"
#include "core/Mesh.h"
#include "math/Mat4.h"
#include "render/Rasterizer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct AppState {
    Camera camera;
    Rasterizer rasterizer;
    Mesh cube;
    bool running = true;
    bool wireframe = false;
    bool orbiting = false;       // 右键：绕场景旋转相机
    bool modelRotating = false;  // 中键：绕模型中心旋转物体
    int lastMouseX = 0;
    int lastMouseY = 0;
    float angle = 0.f;         // 自动旋转（绕世界 Y）
    Mat4 modelRot = Mat4::identity();  // 中键手动旋转（增量累积，无角度限制）
    float modelRotateSpeed = 0.01f;
    int width = 960;
    int height = 720;
    HDC memDC = nullptr;
    HBITMAP dib = nullptr;
    void* dibBits = nullptr;
};

AppState* gApp = nullptr;

void updateWindowTitle(HWND hwnd, const AppState& app) {
    wchar_t title[256]{};
    if (app.rasterizer.materials.empty()) {
        swprintf(title, 256, L"Soft Rasterizer | 无材质 | ↑↓切换 F线框 M:MSAA N:法线");
    } else {
        swprintf(title, 256, L"Soft Rasterizer | 材质: %hs (%d/%zu) | ↑↓切换 F线框 M:MSAA N:法线",
                 app.rasterizer.materials.currentName().c_str(),
                 app.rasterizer.materials.currentIndex + 1, app.rasterizer.materials.count());
    }
    SetWindowTextW(hwnd, title);
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
    bmi.bmiHeader.biHeight = -app.height;  // top-down DIB
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
    if (GetAsyncKeyState('A') & 0x8000) app.camera.moveRight(-dt);
    if (GetAsyncKeyState('D') & 0x8000) app.camera.moveRight(dt);
    if (GetAsyncKeyState('Q') & 0x8000) app.camera.moveUp(-dt);
    if (GetAsyncKeyState('E') & 0x8000) app.camera.moveUp(dt);

    if (!app.orbiting && !app.modelRotating) {
        app.angle += dt * 0.8f;
    }
    // 自动旋转（世界 Y）× 手动旋转（模型局部坐标，可无限累积）
    app.rasterizer.model = Mat4::rotateY(app.angle) * app.modelRot;
    app.rasterizer.mode = app.wireframe ? RenderMode::Wireframe : RenderMode::Solid;

    app.rasterizer.fb.clear();
    const float aspect =
        static_cast<float>(app.width) / static_cast<float>(app.height > 0 ? app.height : 1);
    app.rasterizer.render(app.cube, app.camera, aspect);
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
            return 1;  // 避免背景擦除覆盖我们的 BitBlt

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
            if (wParam == VK_UP) {
                app.rasterizer.prevMaterial();
                updateWindowTitle(hwnd, app);
            }
            if (wParam == VK_DOWN) {
                app.rasterizer.nextMaterial();
                updateWindowTitle(hwnd, app);
            }
            return 0;

        case WM_RBUTTONDOWN:
            app.orbiting = true;
            app.lastMouseX = GET_X_LPARAM(lParam);
            app.lastMouseY = GET_Y_LPARAM(lParam);
            SetCapture(hwnd);
            return 0;

        case WM_RBUTTONUP:
            app.orbiting = false;
            if (!app.modelRotating) ReleaseCapture();
            return 0;

        case WM_MBUTTONDOWN:
            app.modelRotating = true;
            app.lastMouseX = GET_X_LPARAM(lParam);
            app.lastMouseY = GET_Y_LPARAM(lParam);
            SetCapture(hwnd);
            return 0;

        case WM_MBUTTONUP:
            app.modelRotating = false;
            if (!app.orbiting) ReleaseCapture();
            return 0;

        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const float dx = static_cast<float>(x - app.lastMouseX);
            const float dy = static_cast<float>(y - app.lastMouseY);

            if (app.orbiting) {
                app.camera.rotate(dx, dy);
            }
            if (app.modelRotating) {
                const Mat4 delta = Mat4::rotateY(dx * app.modelRotateSpeed) *
                                   Mat4::rotateX(-dy * app.modelRotateSpeed);
                // 在模型局部空间累积旋转，绕几何中心（原点），无俯仰角限制
                app.modelRot = app.modelRot * delta;
            }
            if (app.orbiting || app.modelRotating) {
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
    app.cube = Mesh::createCube(1.2f);
    app.camera.syncOrbitPosition();
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
