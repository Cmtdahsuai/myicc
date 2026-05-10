#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <string>
#include <fstream>
#include "color_controller.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ---- Slider IDs ----
#define ID_SATURATION   1001
#define ID_BRIGHTNESS   1002
#define ID_CONTRAST     1003
#define ID_TEMPERATURE  1004
#define ID_GAMMA        1005
#define ID_LBL_SAT      1011
#define ID_LBL_BRI      1012
#define ID_LBL_CON      1013
#define ID_LBL_TMP      1014
#define ID_LBL_GAM      1015
#define ID_BTN_APPLY    2001
#define ID_BTN_RESET    2002
#define ID_BTN_SAVE     2003
#define ID_STATUS       3001

HINSTANCE g_hInst;
HWND g_hWnd = nullptr;
HWND g_hSliders[5] = {};
HWND g_hLabels[5] = {};
HWND g_hStatus;

ColorParams g_params;
bool g_dirty = false;

// ---- Config Save/Load (simple key-value file) ----
static const wchar_t* CONFIG_PATH = L"myicc_config.txt";

void LoadConfig() {
    FILE* f = _wfopen(CONFIG_PATH, L"r");
    if (!f) return;
    char key[64];
    int val;
    while (fscanf(f, "%63s %d", key, &val) == 2) {
        if (!strcmp(key, "saturation"))  g_params.saturation  = val;
        if (!strcmp(key, "brightness"))  g_params.brightness  = val;
        if (!strcmp(key, "contrast"))    g_params.contrast    = val;
        if (!strcmp(key, "temperature")) g_params.temperature = val;
        if (!strcmp(key, "gamma"))       g_params.gamma       = val;
    }
    fclose(f);
}

void SaveConfig() {
    FILE* f = _wfopen(CONFIG_PATH, L"w");
    if (!f) return;
    fprintf(f, "saturation %d\n",  g_params.saturation);
    fprintf(f, "brightness %d\n",  g_params.brightness);
    fprintf(f, "contrast %d\n",    g_params.contrast);
    fprintf(f, "temperature %d\n", g_params.temperature);
    fprintf(f, "gamma %d\n",       g_params.gamma);
    fclose(f);
}

// ---- Apply settings to GPU ----
void ApplyToGPU() {
    if (!ColorController::Instance().IsReady()) return;
    ColorController::Instance().ApplySettings(g_params);
    g_dirty = false;

    wchar_t status[128];
    swprintf(status, 128, L"Applied: Vibrance=%d  Bright=%d  Contrast=%d  Temp=%d  Gamma=%d",
             g_params.saturation, g_params.brightness, g_params.contrast,
             g_params.temperature, g_params.gamma);
    SetWindowText(g_hStatus, status);
}

void ResetToDefault() {
    g_params = ColorParams();  // all 50 = neutral
    g_dirty = true;

    for (int i = 0; i < 5; i++)
        SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, 50);

    wchar_t buf[16];
    for (int i = 0; i < 5; i++) {
        swprintf(buf, 16, L"%d", 50);
        SetWindowText(g_hLabels[i], buf);
    }

    ApplyToGPU();
}

void SyncSliderToLabel(int sliderIdx, int value) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%d", value);
    SetWindowText(g_hLabels[sliderIdx], buf);
    g_dirty = true;
}

// ---- Window Procedure ----
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hi = ((LPCREATESTRUCT)lParam)->hInstance;

        // Create title labels and sliders
        const wchar_t* names[] = {
            L"Digital Vibrance (Saturation):",
            L"Brightness:",
            L"Contrast:",
            L"Color Temperature:",
            L"Gamma:"
        };
        int ids[] = { ID_SATURATION, ID_BRIGHTNESS, ID_CONTRAST, ID_TEMPERATURE, ID_GAMMA };
        int labelIds[] = { ID_LBL_SAT, ID_LBL_BRI, ID_LBL_CON, ID_LBL_TMP, ID_LBL_GAM };

        for (int i = 0; i < 5; i++) {
            // Name label
            CreateWindow(L"STATIC", names[i],
                         WS_CHILD | WS_VISIBLE,
                         20, 15 + i * 55, 220, 18,
                         hwnd, nullptr, hi, nullptr);

            // Slider
            g_hSliders[i] = CreateWindow(TRACKBAR_CLASS, nullptr,
                         WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                         20, 33 + i * 55, 370, 28,
                         hwnd, (HMENU)(UINT_PTR)ids[i], hi, nullptr);
            SendMessage(g_hSliders[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, 50);

            // Value label
            g_hLabels[i] = CreateWindow(L"STATIC", L"50",
                         WS_CHILD | WS_VISIBLE | SS_CENTER,
                         400, 33 + i * 55, 40, 20,
                         hwnd, (HMENU)(UINT_PTR)labelIds[i], hi, nullptr);
        }

        // Buttons
        CreateWindow(L"BUTTON", L"Apply",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     20, 300, 80, 28, hwnd, (HMENU)ID_BTN_APPLY, hi, nullptr);
        CreateWindow(L"BUTTON", L"Reset Default",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     110, 300, 100, 28, hwnd, (HMENU)ID_BTN_RESET, hi, nullptr);
        CreateWindow(L"BUTTON", L"Save Config",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     220, 300, 90, 28, hwnd, (HMENU)ID_BTN_SAVE, hi, nullptr);

        // Status bar
        g_hStatus = CreateWindow(L"STATIC", L"Ready - Saturation via DWM matrix, others via gamma LUT",
                     WS_CHILD | WS_VISIBLE | SS_LEFT | WS_BORDER,
                     10, 345, 465, 22,
                     hwnd, (HMENU)ID_STATUS, hi, nullptr);

        // Apply initial config
        LoadConfig();
        for (int i = 0; i < 5; i++) {
            int vals[] = { g_params.saturation, g_params.brightness,
                          g_params.contrast, g_params.temperature, g_params.gamma };
            SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, vals[i]);
            SyncSliderToLabel(i, vals[i]);
        }
        ApplyToGPU();

        return 0;
    }

    case WM_HSCROLL: {
        int id = GetDlgCtrlID((HWND)lParam);
        int pos = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

        switch (id) {
            case ID_SATURATION:  g_params.saturation  = pos; SyncSliderToLabel(0, pos); break;
            case ID_BRIGHTNESS:  g_params.brightness  = pos; SyncSliderToLabel(1, pos); break;
            case ID_CONTRAST:    g_params.contrast    = pos; SyncSliderToLabel(2, pos); break;
            case ID_TEMPERATURE: g_params.temperature = pos; SyncSliderToLabel(3, pos); break;
            case ID_GAMMA:       g_params.gamma       = pos; SyncSliderToLabel(4, pos); break;
        }
        ApplyToGPU();  // real-time update on drag
        break;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
            case ID_BTN_APPLY:
                ApplyToGPU();
                break;
            case ID_BTN_RESET:
                ResetToDefault();
                break;
            case ID_BTN_SAVE:
                SaveConfig();
                SetWindowText(g_hStatus, L"Config saved.");
                break;
        }
        break;
    }

    case WM_CLOSE:
        ColorController::Instance().Shutdown();
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---- Entry Point ----
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_hInst = hInstance;

    // Init NVAPI + color controller
    if (!ColorController::Instance().Initialize()) {
        MessageBox(nullptr, L"Failed to initialize NVAPI.\n\n"
                   L"Make sure you have an NVIDIA GPU and the driver is installed.",
                   L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Init common controls
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MyICCWindow";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassEx(&wc);

    // Create window
    RECT r = { 0, 0, 480, 420 };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    g_hWnd = CreateWindowEx(0, L"MyICCWindow", L"MyICC - GPU Color Control",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top,
                            nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) return 1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
