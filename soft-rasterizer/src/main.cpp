#include "core/Camera.h"
#include "core/Mesh.h"
#include "core/SceneObject.h"
#include "math/Mat4.h"
#include "render/ColorGrade.h"
#include "render/Rasterizer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr float kFloorSize = 10.f;
constexpr float kCubeBaseY = 0.68f;
constexpr float kWallSize = 10.f;
constexpr float kWallCornerX = -5.f;
constexpr float kWallCornerZ = -5.f;
constexpr float kWindowWidth = 4.f;
constexpr float kWindowHeight = 3.f;
constexpr uint32_t kBackgroundWhite = 0xFFFFFFFF;
constexpr float kCubeTransparentAlpha = 0.5f;

constexpr int kSliderRowHeight = 28;
constexpr int kSliderMargin = 10;
constexpr int kSliderLabelWidth = 56;
constexpr int kSliderTrackWidth = 200;
constexpr int kSliderPanelPadding = 8;
constexpr int kSliderTrackHeight = 8;
constexpr int kSliderThumbSize = 14;

struct SliderUi {
    float brightness = 0.5f;
    float saturation = 0.5f;
    float contrast = 0.5f;
    int draggingIndex = -1;
    RECT panelRect{};
    RECT trackRects[3]{};
};

struct AppState {
    Camera camera;
    Rasterizer rasterizer;
    ColorGrade colorGrade;
    SliderUi sliderUi;
    Mesh floor;
    Mesh cube;
    Mesh bgSphere;
    Mesh lightSphere;
    Mesh wallX;
    Mesh wallZ;
    std::vector<SceneObject> scene;
    bool running = true;
    bool wireframe = false;
    bool cubeTransparent = false;
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
    HFONT uiFont = nullptr;
};

AppState* gApp = nullptr;

void layoutSliderUi(AppState& app) {
    const int clientW = app.width;
    const int clientH = app.height;
    const int trackWidth =
        std::min(kSliderTrackWidth, std::max(120, clientW - kSliderMargin * 2 - kSliderLabelWidth));
    const int panelW = kSliderLabelWidth + trackWidth + kSliderPanelPadding * 2;
    const int panelH = kSliderRowHeight * 3 + kSliderPanelPadding * 2;
    const int x0 = kSliderMargin;
    const int y0 = std::max(kSliderMargin, clientH - kSliderMargin - panelH);

    app.sliderUi.panelRect = {x0, y0, x0 + panelW, y0 + panelH};

    for (int row = 0; row < 3; ++row) {
        const int rowY = y0 + kSliderPanelPadding + row * kSliderRowHeight;
        const int trackY = rowY + (kSliderRowHeight - kSliderTrackHeight) / 2;
        app.sliderUi.trackRects[row] = {x0 + kSliderLabelWidth + kSliderPanelPadding, trackY,
                                        x0 + kSliderLabelWidth + kSliderPanelPadding + trackWidth,
                                        trackY + kSliderTrackHeight};
    }
}

bool pointInRect(int x, int y, const RECT& rc) {
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

int hitTestSliderUi(const AppState& app, int x, int y) {
    if (!pointInRect(x, y, app.sliderUi.panelRect)) return -1;
    for (int i = 0; i < 3; ++i) {
        const RECT& track = app.sliderUi.trackRects[i];
        RECT hit = track;
        InflateRect(&hit, 0, 8);
        if (pointInRect(x, y, hit)) return i;
    }
    return -1;
}

void setSliderValue(AppState& app, int index, int mouseX) {
    if (index < 0 || index > 2) return;
    const RECT& track = app.sliderUi.trackRects[index];
    const int trackW = track.right - track.left;
    if (trackW <= 0) return;

    float t = static_cast<float>(mouseX - track.left) / static_cast<float>(trackW);
    t = std::clamp(t, 0.f, 1.f);
    switch (index) {
        case 0: app.sliderUi.brightness = t; break;
        case 1: app.sliderUi.saturation = t; break;
        default: app.sliderUi.contrast = t; break;
    }
}

void syncColorGradeFromSliderUi(AppState& app) {
    auto toPos = [](float t) {
        return static_cast<int>(std::clamp(t, 0.f, 1.f) * 100.f + 0.5f);
    };
    app.colorGrade.brightness = ColorGrade::sliderToBrightness(toPos(app.sliderUi.brightness));
    app.colorGrade.saturation = ColorGrade::sliderToSaturation(toPos(app.sliderUi.saturation));
    app.colorGrade.contrast = ColorGrade::sliderToContrast(toPos(app.sliderUi.contrast));
}

void drawSliderOverlay(HDC dc, AppState& app) {
    layoutSliderUi(app);

    const RECT& panel = app.sliderUi.panelRect;
    HBRUSH panelBrush = CreateSolidBrush(RGB(32, 32, 38));
    HPEN panelPen = CreatePen(PS_SOLID, 1, RGB(90, 90, 100));
    HGDIOBJ oldBrush = SelectObject(dc, panelBrush);
    HGDIOBJ oldPen = SelectObject(dc, panelPen);
    RoundRect(dc, panel.left, panel.top, panel.right, panel.bottom, 10, 10);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(panelPen);
    DeleteObject(panelBrush);

    HBRUSH trackBrush = CreateSolidBrush(RGB(88, 88, 96));
    HBRUSH thumbBrush = CreateSolidBrush(RGB(235, 235, 240));
    HPEN thumbPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));

    const wchar_t* labels[] = {L"亮度", L"饱和度", L"对比度"};
    const float values[] = {app.sliderUi.brightness, app.sliderUi.saturation, app.sliderUi.contrast};

    HFONT oldFont = nullptr;
    if (app.uiFont) {
        oldFont = reinterpret_cast<HFONT>(SelectObject(dc, app.uiFont));
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(240, 240, 245));

    for (int i = 0; i < 3; ++i) {
        const RECT& track = app.sliderUi.trackRects[i];
        const int rowY = panel.top + kSliderPanelPadding + i * kSliderRowHeight;

        RECT labelRect = {panel.left + kSliderPanelPadding, rowY, panel.left + kSliderLabelWidth,
                          rowY + kSliderRowHeight};
        DrawTextW(dc, labels[i], -1, &labelRect, DT_VCENTER | DT_LEFT | DT_SINGLELINE);

        SelectObject(dc, trackBrush);
        SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, track.left, track.top, track.right, track.bottom, 4, 4);

        const int trackW = track.right - track.left;
        const int thumbX = track.left + static_cast<int>(values[i] * static_cast<float>(trackW));
        const int thumbY = (track.top + track.bottom) / 2;
        const int halfThumb = kSliderThumbSize / 2;

        SelectObject(dc, thumbBrush);
        SelectObject(dc, thumbPen);
        Ellipse(dc, thumbX - halfThumb, thumbY - halfThumb, thumbX + halfThumb, thumbY + halfThumb);
    }

    if (oldFont) SelectObject(dc, oldFont);
    DeleteObject(trackBrush);
    DeleteObject(thumbBrush);
    DeleteObject(thumbPen);
}

void updateWindowTitle(HWND hwnd, const AppState& app) {
    wchar_t title[384]{};
    const wchar_t* shadowState = app.rasterizer.shadowSceneEnabled ? L"开" : L"关";
    const wchar_t* transparentState = app.cubeTransparent ? L"开" : L"关";
    const wchar_t* bloomState = app.rasterizer.bloomEnabled ? L"开" : L"关";
    if (app.rasterizer.materials.empty()) {
        swprintf(title, 384,
                 L"Soft Rasterizer | 泛光:%s(B) | 半透明:%s(T) | 阴影:%s(L) | 中键旋转 Shift+中键平移 右键旋转物体",
                 bloomState, transparentState, shadowState);
    } else {
        swprintf(title, 384,
                 L"Soft Rasterizer | 材质:%hs (%d/%zu) | 泛光:%s(B) | 半透明:%s(T) | 阴影:%s(L) | 右键旋转 WASD/QE移光源",
                 app.rasterizer.materials.currentName().c_str(),
                 app.rasterizer.materials.currentIndex + 1, app.rasterizer.materials.count(),
                 bloomState, transparentState, shadowState);
    }
    SetWindowTextW(hwnd, title);
}

void rebuildScene(AppState& app) {
    app.scene.clear();

    SceneObject floorObj;
    floorObj.mesh = &app.floor;
    floorObj.model = Mat4::identity();
    floorObj.material = ObjectMaterial::Flat;
    floorObj.flatColor = {0.55f, 0.55f, 0.58f};
    floorObj.castShadow = true;
    floorObj.receiveShadow = true;
    app.scene.push_back(floorObj);

    SceneObject wallXObj;
    wallXObj.mesh = &app.wallX;
    wallXObj.model = Mat4::translate({kWallCornerX, 0.f, kWallCornerZ});
    wallXObj.material = ObjectMaterial::Flat;
    wallXObj.flatColor = {0.55f, 0.55f, 0.58f};
    wallXObj.castShadow = true;
    wallXObj.receiveShadow = true;
    app.scene.push_back(wallXObj);

    SceneObject wallZObj;
    wallZObj.mesh = &app.wallZ;
    wallZObj.model = Mat4::translate({kWallCornerX, 0.f, kWallCornerZ});
    wallZObj.material = ObjectMaterial::Flat;
    wallZObj.flatColor = {0.55f, 0.55f, 0.58f};
    wallZObj.castShadow = true;
    wallZObj.receiveShadow = true;
    app.scene.push_back(wallZObj);

    SceneObject cubeObj;
    cubeObj.mesh = &app.cube;
    cubeObj.model =
        Mat4::translate({0.f, kCubeBaseY, 0.f}) * Mat4::rotateY(app.angle) * app.modelRot;
    cubeObj.material = ObjectMaterial::Textured;
    cubeObj.alpha = app.cubeTransparent ? kCubeTransparentAlpha : 1.f;
    cubeObj.castShadowStrength = app.cubeTransparent ? kCubeTransparentAlpha : 1.f;
    cubeObj.castShadow = true;
    cubeObj.receiveShadow = true;
    app.scene.push_back(cubeObj);

    SceneObject sphereObj;
    sphereObj.mesh = &app.bgSphere;
    sphereObj.model = Mat4::translate({-1.2f, 0.55f, -1.2f});
    sphereObj.material = ObjectMaterial::Flat;
    sphereObj.flatColor = {0.3f, 0.7f, 0.6f};
    sphereObj.castShadow = true;
    sphereObj.receiveShadow = true;
    app.scene.push_back(sphereObj);

    SceneObject lightObj;
    lightObj.mesh = &app.lightSphere;
    lightObj.model = Mat4::translate(app.rasterizer.pointLight.position);
    lightObj.material = ObjectMaterial::Emissive;
    lightObj.emissiveColor = {1.f, 0.95f, 0.75f};
    lightObj.castShadow = false;
    lightObj.receiveShadow = false;
    app.scene.push_back(lightObj);
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
    layoutSliderUi(app);
    return true;
}

void present(HWND hwnd, AppState& app) {
    if (!app.dibBits || !app.memDC) return;

    HDC windowDC = GetDC(hwnd);
    if (!windowDC) return;

    memcpy(app.dibBits, app.rasterizer.fb.color.data(),
           static_cast<size_t>(app.width) * app.height * sizeof(uint32_t));
    BitBlt(windowDC, 0, 0, app.width, app.height, app.memDC, 0, 0, SRCCOPY);
    drawSliderOverlay(windowDC, app);
    ReleaseDC(hwnd, windowDC);
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

    if (!app.cameraOrbiting && !app.cameraPanning && !app.modelRotating &&
        app.sliderUi.draggingIndex < 0) {
        app.angle += dt * 0.8f;
    }

    app.rasterizer.model = Mat4::rotateY(app.angle);
    app.rasterizer.mode = app.wireframe ? RenderMode::Wireframe : RenderMode::Solid;

    rebuildScene(app);
    app.rasterizer.fb.clear(kBackgroundWhite);
    const float aspect =
        static_cast<float>(app.width) / static_cast<float>(app.height > 0 ? app.height : 1);
    app.rasterizer.renderScene(app.scene, app.camera, aspect);
    syncColorGradeFromSliderUi(app);
    app.colorGrade.apply(app.rasterizer.fb);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState& app = *gApp;

    switch (msg) {
        case WM_CREATE: {
            HDC hdc = GetDC(hwnd);
            const bool ok = hdc && createBackBuffer(app, hdc);
            ReleaseDC(hwnd, hdc);
            if (!ok) return -1;

            app.uiFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            app.rasterizer.loadMaterials();
            syncColorGradeFromSliderUi(app);
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
            present(hwnd, app);
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
            if (wParam == 'T') {
                app.cubeTransparent = !app.cubeTransparent;
                updateWindowTitle(hwnd, app);
            }
            if (wParam == 'B') {
                app.rasterizer.bloomEnabled = !app.rasterizer.bloomEnabled;
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

        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int hit = hitTestSliderUi(app, x, y);
            if (hit >= 0) {
                app.sliderUi.draggingIndex = hit;
                setSliderValue(app, hit, x);
                syncColorGradeFromSliderUi(app);
                SetCapture(hwnd);
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP:
            if (app.sliderUi.draggingIndex >= 0) {
                app.sliderUi.draggingIndex = -1;
                ReleaseCapture();
                return 0;
            }
            break;

        case WM_MBUTTONDOWN:
            app.lastMouseX = GET_X_LPARAM(lParam);
            app.lastMouseY = GET_Y_LPARAM(lParam);
            if (hitTestSliderUi(app, app.lastMouseX, app.lastMouseY) >= 0) return 0;
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
            if (hitTestSliderUi(app, app.lastMouseX, app.lastMouseY) >= 0) return 0;
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

            if (app.sliderUi.draggingIndex >= 0) {
                setSliderValue(app, app.sliderUi.draggingIndex, x);
                syncColorGradeFromSliderUi(app);
                return 0;
            }

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
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            POINT pt{x, y};
            ScreenToClient(hwnd, &pt);
            if (hitTestSliderUi(app, pt.x, pt.y) >= 0) return 0;

            const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam));
            app.camera.scrollZoom(delta);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (app.uiFont) DeleteObject(app.uiFont);
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
    app.bgSphere = Mesh::createSphere(0.32f, 24, 16);
    app.lightSphere = Mesh::createSphere(0.12f, 14, 10);
    app.wallX = Mesh::createWallWithWindow(kWallSize, kWallSize, kWindowWidth, kWindowHeight,
                                           Mesh::WallFacing::PosX);
    app.wallZ = Mesh::createWall(kWallSize, kWallSize, Mesh::WallFacing::PosZ);

    const Vec3 kObjectCenter{0.f, kCubeBaseY, 0.f};
    const Vec3 kCameraPos{2.5f, 3.2f, 2.5f};
    app.camera.setView(kObjectCenter, kCameraPos);

    app.rasterizer.lightTarget = {0.f, kCubeBaseY, 0.f};
    app.rasterizer.pointLight.position = {1.5f, 3.f, 1.3f};

    gApp = &app;

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SoftRasterizerWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&wc);

    const int winW = app.width + GetSystemMetrics(SM_CXFIXEDFRAME) * 2;
    const int winH = app.height + GetSystemMetrics(SM_CYFIXEDFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION);

    HWND hwnd = CreateWindowW(L"SoftRasterizerWnd", L"Soft Rasterizer",
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
