#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cstdlib>
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
#define ID_EDIT_SAT     1021
#define ID_EDIT_BRI     1022
#define ID_EDIT_CON     1023
#define ID_EDIT_TMP     1024
#define ID_EDIT_GAM     1025
#define ID_BTN_APPLY    2001
#define ID_BTN_RESET    2002
#define ID_BTN_SAVE     2003
#define ID_STATUS       3001

HINSTANCE g_hInst;
HWND g_hWnd = nullptr;
HWND g_hSliders[5] = {};
HWND g_hEdits[5] = {};
HWND g_hStatus;

ColorParams g_params;
bool g_dirty = false;
bool g_updating = false;  // prevent edit<->slider recursion

// ---- Config Save/Load ----
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

void ApplyToGPU() {
    if (!ColorController::Instance().IsReady()) return;
    ColorController::Instance().ApplySettings(g_params);
    g_dirty = false;

    wchar_t status[128];
    swprintf(status, 128, L"已应用: 饱和度=%d  亮度=%d  对比度=%d  色温=%d  伽马=%d",
             g_params.saturation, g_params.brightness, g_params.contrast,
             g_params.temperature, g_params.gamma);
    SetWindowText(g_hStatus, status);
}

void ResetToDefault() {
    g_params = ColorParams();

    g_updating = true;
    for (int i = 0; i < 5; i++) {
        SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, 50);
        SetWindowText(g_hEdits[i], L"50");
    }
    g_updating = false;

    ApplyToGPU();
}

int ClampValue(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

// Called when user manually types in an edit box
void OnEditChanged(int idx, int editId) {
    if (g_updating) return;

    wchar_t buf[16];
    GetWindowText(g_hEdits[idx], buf, 16);
    int val = _wtoi(buf);
    val = ClampValue(val);

    g_updating = true;

    // Update slider and edit to clamped value
    SendMessage(g_hSliders[idx], TBM_SETPOS, TRUE, val);
    if (val != _wtoi(buf)) {
        swprintf(buf, 16, L"%d", val);
        SetWindowText(g_hEdits[idx], buf);
    }

    switch (idx) {
        case 0: g_params.saturation  = val; break;
        case 1: g_params.brightness  = val; break;
        case 2: g_params.contrast    = val; break;
        case 3: g_params.temperature = val; break;
        case 4: g_params.gamma       = val; break;
    }

    g_updating = false;
    ApplyToGPU();
}

void SyncEditToSlider(int idx, int value) {
    if (g_updating) return;
    g_updating = true;
    wchar_t buf[16];
    swprintf(buf, 16, L"%d", value);
    SetWindowText(g_hEdits[idx], buf);
    g_updating = false;
}

// ---- Window Procedure ----
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hi = ((LPCREATESTRUCT)lParam)->hInstance;

        const wchar_t* names[] = {
            L"饱和度:",
            L"亮度:",
            L"对比度:",
            L"色温:",
            L"伽马:"
        };
        int sliderIds[] = { ID_SATURATION, ID_BRIGHTNESS, ID_CONTRAST, ID_TEMPERATURE, ID_GAMMA };
        int editIds[]   = { ID_EDIT_SAT, ID_EDIT_BRI, ID_EDIT_CON, ID_EDIT_TMP, ID_EDIT_GAM };

        for (int i = 0; i < 5; i++) {
            // Name label
            CreateWindow(L"STATIC", names[i],
                         WS_CHILD | WS_VISIBLE,
                         10, 12 + i * 55, 60, 20,
                         hwnd, nullptr, hi, nullptr);

            // Slider
            g_hSliders[i] = CreateWindow(TRACKBAR_CLASS, nullptr,
                         WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                         75, 30 + i * 55, 310, 28,
                         hwnd, (HMENU)(UINT_PTR)sliderIds[i], hi, nullptr);
            SendMessage(g_hSliders[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, 50);

            // Editable value box
            g_hEdits[i] = CreateWindow(L"EDIT", L"50",
                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
                         395, 30 + i * 55, 45, 22,
                         hwnd, (HMENU)(UINT_PTR)editIds[i], hi, nullptr);
        }

        // Buttons
        CreateWindow(L"BUTTON", L"应用",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     10, 300, 80, 28, hwnd, (HMENU)ID_BTN_APPLY, hi, nullptr);
        CreateWindow(L"BUTTON", L"恢复默认",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     100, 300, 90, 28, hwnd, (HMENU)ID_BTN_RESET, hi, nullptr);
        CreateWindow(L"BUTTON", L"保存配置",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     200, 300, 80, 28, hwnd, (HMENU)ID_BTN_SAVE, hi, nullptr);

        // Status bar
        g_hStatus = CreateWindow(L"STATIC", L"就绪 - 饱和度通过DWM矩阵, 其余通过gamma LUT",
                     WS_CHILD | WS_VISIBLE | SS_LEFT | WS_BORDER,
                     10, 345, 460, 22,
                     hwnd, (HMENU)ID_STATUS, hi, nullptr);

        // Load saved config and apply
        LoadConfig();
        g_updating = true;
        for (int i = 0; i < 5; i++) {
            int vals[] = { g_params.saturation, g_params.brightness,
                          g_params.contrast, g_params.temperature, g_params.gamma };
            SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, vals[i]);
            wchar_t buf[16];
            swprintf(buf, 16, L"%d", vals[i]);
            SetWindowText(g_hEdits[i], buf);
        }
        g_updating = false;
        ApplyToGPU();

        return 0;
    }

    case WM_HSCROLL: {
        int id = GetDlgCtrlID((HWND)lParam);
        int pos = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

        switch (id) {
            case ID_SATURATION:  g_params.saturation  = pos; SyncEditToSlider(0, pos); break;
            case ID_BRIGHTNESS:  g_params.brightness  = pos; SyncEditToSlider(1, pos); break;
            case ID_CONTRAST:    g_params.contrast    = pos; SyncEditToSlider(2, pos); break;
            case ID_TEMPERATURE: g_params.temperature = pos; SyncEditToSlider(3, pos); break;
            case ID_GAMMA:       g_params.gamma       = pos; SyncEditToSlider(4, pos); break;
        }
        ApplyToGPU();
        break;
    }

    case WM_COMMAND: {
        int ctrlId = LOWORD(wParam);
        int notify = HIWORD(wParam);

        // Edit box changed (user typed and pressed Enter or lost focus)
        if (notify == EN_KILLFOCUS) {
            switch (ctrlId) {
                case ID_EDIT_SAT: OnEditChanged(0, ctrlId); break;
                case ID_EDIT_BRI: OnEditChanged(1, ctrlId); break;
                case ID_EDIT_CON: OnEditChanged(2, ctrlId); break;
                case ID_EDIT_TMP: OnEditChanged(3, ctrlId); break;
                case ID_EDIT_GAM: OnEditChanged(4, ctrlId); break;
            }
            break;
        }

        switch (ctrlId) {
            case ID_BTN_APPLY:
                ApplyToGPU();
                break;
            case ID_BTN_RESET:
                ResetToDefault();
                break;
            case ID_BTN_SAVE:
                SaveConfig();
                SetWindowText(g_hStatus, L"配置已保存");
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

    if (!ColorController::Instance().Initialize()) {
        MessageBox(nullptr,
                   L"初始化失败。\n\n请确认已安装 NVIDIA 显卡驱动，或尝试以管理员身份运行。",
                   L"错误", MB_ICONERROR | MB_OK);
        return 1;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MyICCWindow";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassEx(&wc);

    RECT r = { 0, 0, 480, 420 };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    g_hWnd = CreateWindowEx(0, L"MyICCWindow", L"显示器色彩调节 - myICC",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top,
                            nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) return 1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
