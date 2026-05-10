#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include "color_controller.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ---- IDs ----
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
#define ID_BTN_RESET    2002
#define ID_BTN_PICK     2004
#define ID_BTN_CLEAR    2005
#define ID_STATUS       3001
#define ID_TARGET_LBL   3002
#define ID_TIMER        1

HINSTANCE g_hInst;
HWND g_hWnd = nullptr;
HWND g_hSliders[5] = {};
HWND g_hEdits[5] = {};
HWND g_hStatus;
HWND g_hTargetLabel;

ColorParams g_params;
bool g_dirty = false;
bool g_updating = false;
bool g_effectActive = false;
wchar_t g_targetPath[MAX_PATH] = {};
wchar_t g_targetNameOnly[MAX_PATH] = {};
static const wchar_t* CONFIG_PATH = L"myicc_config.txt";

// ---- Config ----
void AutoSaveConfig() {
    FILE* f = _wfopen(CONFIG_PATH, L"w");
    if (!f) return;
    fprintf(f, "saturation %d\n",  g_params.saturation);
    fprintf(f, "brightness %d\n",  g_params.brightness);
    fprintf(f, "contrast %d\n",    g_params.contrast);
    fprintf(f, "temperature %d\n", g_params.temperature);
    fprintf(f, "gamma %d\n",       g_params.gamma);
    if (g_targetPath[0]) {
        fprintf(f, "target_path %ls\n", g_targetPath);
    }
    fclose(f);
}

void LoadConfig() {
    FILE* f = _wfopen(CONFIG_PATH, L"r");
    if (!f) return;
    wchar_t line[512];
    while (fgetws(line, 512, f)) {
        wchar_t key[64];
        int val = 0;
        if (swscanf(line, L"%63s %d", key, &val) == 2) {
            if (!wcscmp(key, L"saturation"))  g_params.saturation  = val;
            if (!wcscmp(key, L"brightness"))  g_params.brightness  = val;
            if (!wcscmp(key, L"contrast"))    g_params.contrast    = val;
            if (!wcscmp(key, L"temperature")) g_params.temperature = val;
            if (!wcscmp(key, L"gamma"))       g_params.gamma       = val;
        }
        wchar_t pathStr[MAX_PATH];
        if (swscanf(line, L"target_path %[^\r\n]", pathStr) == 1) {
            wcscpy(g_targetPath, pathStr);
        }
    }
    fclose(f);

    // Extract filename for display
    if (g_targetPath[0]) {
        wchar_t* lastSlash = wcsrchr(g_targetPath, L'\\');
        wcscpy(g_targetNameOnly, lastSlash ? lastSlash + 1 : g_targetPath);
    }
}

// ---- Foreground detection ----
bool IsTargetForeground() {
    if (!g_targetPath[0]) return false;
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return false;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return false;

    wchar_t path[MAX_PATH];
    DWORD len = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(hProc, 0, path, &len);
    CloseHandle(hProc);

    if (!ok) return false;
    return _wcsicmp(path, g_targetPath) == 0;
}

// ---- Apply / Reset effects ----
void ApplyToGPU() {
    if (!ColorController::Instance().IsReady()) return;
    ColorController::Instance().ApplySettings(g_params);
    g_effectActive = true;
    g_dirty = false;

    wchar_t status[128];
    swprintf(status, 128, L"已应用: 饱和度=%d  亮度=%d  对比度=%d  色温=%d  伽马=%d",
             g_params.saturation, g_params.brightness, g_params.contrast,
             g_params.temperature, g_params.gamma);
    SetWindowText(g_hStatus, status);
}

void ResetEffects() {
    ColorParams neutral;
    ColorController::Instance().ApplySettings(neutral);
    g_effectActive = false;
    SetWindowText(g_hStatus, L"滤镜已取消 (当前不在目标程序中)");
}

void ResetToDefault() {
    g_params = ColorParams();

    g_updating = true;
    for (int i = 0; i < 5; i++) {
        SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, 50);
        SetWindowText(g_hEdits[i], L"50");
    }
    g_updating = false;

    AutoSaveConfig();
    ApplyToGPU();
}

int ClampValue(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

void OnEditChanged(int idx) {
    if (g_updating) return;

    wchar_t buf[16];
    GetWindowText(g_hEdits[idx], buf, 16);
    int val = _wtoi(buf);
    val = ClampValue(val);

    g_updating = true;
    SendMessage(g_hSliders[idx], TBM_SETPOS, TRUE, val);
    if (val != _wtoi(buf)) {
        swprintf(buf, 16, L"%d", val);
        SetWindowText(g_hEdits[idx], buf);
    }
    g_updating = false;

    switch (idx) {
        case 0: g_params.saturation  = val; break;
        case 1: g_params.brightness  = val; break;
        case 2: g_params.contrast    = val; break;
        case 3: g_params.temperature = val; break;
        case 4: g_params.gamma       = val; break;
    }

    AutoSaveConfig();
    if (g_effectActive) ApplyToGPU();
}

void SyncEditToSlider(int idx, int value) {
    if (g_updating) return;
    g_updating = true;
    wchar_t buf[16];
    swprintf(buf, 16, L"%d", value);
    SetWindowText(g_hEdits[idx], buf);
    g_updating = false;
}

void UpdateTargetDisplay() {
    if (g_targetPath[0]) {
        wchar_t display[512];
        swprintf(display, 512, L"当前目标: %s", g_targetNameOnly);
        SetWindowText(g_hTargetLabel, display);
    } else {
        SetWindowText(g_hTargetLabel, L"尚未选择目标程序 (全局生效)");
    }
}

void PickTargetApp() {
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = g_targetPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn)) {
        wchar_t* lastSlash = wcsrchr(g_targetPath, L'\\');
        wcscpy(g_targetNameOnly, lastSlash ? lastSlash + 1 : g_targetPath);
        UpdateTargetDisplay();
        AutoSaveConfig();
    }
}

void ClearTarget() {
    g_targetPath[0] = 0;
    g_targetNameOnly[0] = 0;
    UpdateTargetDisplay();
    AutoSaveConfig();
    ApplyToGPU();
}

// ---- Window Procedure ----
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hi = ((LPCREATESTRUCT)lParam)->hInstance;

        const wchar_t* names[] = { L"饱和度:", L"亮度:", L"对比度:", L"色温:", L"伽马:" };
        int sliderIds[] = { ID_SATURATION, ID_BRIGHTNESS, ID_CONTRAST, ID_TEMPERATURE, ID_GAMMA };
        int editIds[]   = { ID_EDIT_SAT, ID_EDIT_BRI, ID_EDIT_CON, ID_EDIT_TMP, ID_EDIT_GAM };

        for (int i = 0; i < 5; i++) {
            CreateWindow(L"STATIC", names[i],
                         WS_CHILD | WS_VISIBLE,
                         10, 12 + i * 55, 60, 20,
                         hwnd, nullptr, hi, nullptr);

            g_hSliders[i] = CreateWindow(TRACKBAR_CLASS, nullptr,
                         WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                         75, 30 + i * 55, 310, 28,
                         hwnd, (HMENU)(UINT_PTR)sliderIds[i], hi, nullptr);
            SendMessage(g_hSliders[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, 50);

            g_hEdits[i] = CreateWindow(L"EDIT", L"50",
                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
                         395, 30 + i * 55, 45, 22,
                         hwnd, (HMENU)(UINT_PTR)editIds[i], hi, nullptr);
        }

        // Target app section
        CreateWindow(L"STATIC", L"滤镜生效条件: 当指定程序处于前台时自动应用",
                     WS_CHILD | WS_VISIBLE | SS_LEFT,
                     10, 290, 420, 16,
                     hwnd, nullptr, hi, nullptr);

        g_hTargetLabel = CreateWindow(L"STATIC", L"尚未选择目标程序 (全局生效)",
                     WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN,
                     10, 308, 310, 22,
                     hwnd, (HMENU)ID_TARGET_LBL, hi, nullptr);

        CreateWindow(L"BUTTON", L"选择程序...",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     325, 306, 90, 24,
                     hwnd, (HMENU)ID_BTN_PICK, hi, nullptr);

        CreateWindow(L"BUTTON", L"清除",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     420, 306, 45, 24,
                     hwnd, (HMENU)ID_BTN_CLEAR, hi, nullptr);

        // Reset button
        CreateWindow(L"BUTTON", L"恢复默认",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     325, 340, 90, 28,
                     hwnd, (HMENU)ID_BTN_RESET, hi, nullptr);

        // Status bar
        g_hStatus = CreateWindow(L"STATIC", L"就绪 - 拖动滑块调整, 选择程序后仅对该程序生效",
                     WS_CHILD | WS_VISIBLE | SS_LEFT | WS_BORDER,
                     10, 380, 460, 22,
                     hwnd, (HMENU)ID_STATUS, hi, nullptr);

        // Load saved config
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
        UpdateTargetDisplay();

        // Start timer for foreground detection
        SetTimer(hwnd, ID_TIMER, 500, nullptr);
        ApplyToGPU();

        return 0;
    }

    case WM_TIMER: {
        static bool wasForeground = false;
        bool isForeground = IsTargetForeground() || !g_targetPath[0];

        if (isForeground && !wasForeground) {
            ApplyToGPU();
        } else if (!isForeground && wasForeground) {
            ResetEffects();
        }
        wasForeground = isForeground;
        break;
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
        AutoSaveConfig();
        if (g_effectActive) ApplyToGPU();
        break;
    }

    case WM_COMMAND: {
        int ctrlId = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if (notify == EN_KILLFOCUS) {
            switch (ctrlId) {
                case ID_EDIT_SAT: OnEditChanged(0); break;
                case ID_EDIT_BRI: OnEditChanged(1); break;
                case ID_EDIT_CON: OnEditChanged(2); break;
                case ID_EDIT_TMP: OnEditChanged(3); break;
                case ID_EDIT_GAM: OnEditChanged(4); break;
            }
            break;
        }

        switch (ctrlId) {
            case ID_BTN_RESET:
                ResetToDefault();
                break;
            case ID_BTN_PICK:
                PickTargetApp();
                break;
            case ID_BTN_CLEAR:
                ClearTarget();
                break;
        }
        break;
    }

    case WM_CLOSE:
        KillTimer(hwnd, ID_TIMER);
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

    RECT r = { 0, 0, 490, 440 };
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
