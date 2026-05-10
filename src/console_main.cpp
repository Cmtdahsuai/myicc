// Console version - mimics exact GUI init path to find the bug
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include "color_controller.h"

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "START\n");
    printf("=== myICC Console Debug ===\n");
    fflush(stdout);

    // Step 1: Init NVAPI
    printf("[1] Initializing NVAPI...\n");
    bool ok = ColorController::Instance().Initialize();
    printf("    Result: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        printf("    -> NVAPI init failed\n");
        return 1;
    }

    // Step 2: Check display
    printf("[2] Checking display...\n");
    auto& nv = NvAPIWrapper::Instance();
    auto displays = nv.GetDisplays();
    printf("    Displays: %zu\n", displays.size());
    for (size_t i = 0; i < displays.size(); i++) {
        printf("    Display %zu: handle=%p primary=%d\n",
               i, displays[i].handle, displays[i].isPrimary);
    }

    NvDisplayHandle h = nv.GetPrimaryHandle();
    printf("    Primary handle: %p\n", h);

    // Step 3: Test GDI gamma
    printf("[3] Testing GDI gamma...\n");
    GDI_GAMMA_RAMP ramp;
    ok = nv.GetGDIGammaRamp(ramp);
    printf("    GetGDIGammaRamp: %s\n", ok ? "OK" : "FAILED");

    // Step 4: Apply custom settings
    printf("[4] Applying color settings...\n");
    ColorParams params;
    params.gamma = 30;   // ~0.7 gamma
    params.brightness = 60;
    params.saturation = 70;  // will fail silently
    ok = ColorController::Instance().ApplySettings(params);
    printf("    ApplySettings: %s\n", ok ? "OK" : "FAILED");

    // Step 5: Now create the GUI window (exact same code as main.cpp)
    printf("[5] Creating GUI window...\n");

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MyICCConsoleTest";

    if (!RegisterClassExW(&wc)) {
        printf("    RegisterClassEx failed: %lu\n", GetLastError());
        return 1;
    }
    printf("    Class registered\n");

    RECT r = { 0, 0, 480, 420 };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(0, L"MyICCConsoleTest", L"Test Window",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                r.right - r.left, r.bottom - r.top,
                                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        printf("    CreateWindowEx failed: %lu\n", GetLastError());
        return 1;
    }
    printf("    Window created: %p\n", hwnd);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    printf("    Window shown\n");

    printf("\n=== GUI should be visible. Check your screen! ===\n");

    // Message loop - keep window alive for 5 seconds
    printf("Window will close in 5 seconds...\n");
    MSG msg;
    DWORD deadline = GetTickCount() + 5000;
    while (GetTickCount() < deadline) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }

    DestroyWindow(hwnd);
    ColorController::Instance().Shutdown();
    return 0;
}
