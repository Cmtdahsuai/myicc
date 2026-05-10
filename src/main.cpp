#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
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
#define ID_BTN_REFRESH  2006
#define ID_BTN_CLEAR    2005
#define ID_COMBO_TARGET 2007
#define ID_STATUS       3001
#define ID_TIMER        1

HINSTANCE g_hInst;
HWND g_hWnd = nullptr;
HWND g_hSliders[5] = {};
HWND g_hEdits[5] = {};
HWND g_hStatus;
HWND g_hCombo;

ColorParams g_params;
bool g_dirty = false;
bool g_updating = false;
bool g_effectActive = false;
wchar_t g_targetPath[MAX_PATH] = {};
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
    if (g_targetPath[0]) fprintf(f, "target_path %ls\n", g_targetPath);
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
}

// ---- Process enumeration ----
struct ProcInfo {
    std::wstring name;
    std::wstring path;
    SIZE_T memKB;
};
std::vector<ProcInfo> g_procList;

void FreeComboData() {
    for (int i = 0; i < (int)SendMessage(g_hCombo, CB_GETCOUNT, 0, 0); i++) {
        void* data = (void*)SendMessage(g_hCombo, CB_GETITEMDATA, i, 0);
        if (data) free(data);
    }
}

void RefreshProcessList() {
    SendMessage(g_hCombo, CB_RESETCONTENT, 0, 0);
    FreeComboData();
    g_procList.clear();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == 0) continue;

            HANDLE hProc = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
            if (!hProc) continue;

            wchar_t path[MAX_PATH] = {};
            DWORD len = MAX_PATH;
            bool gotPath = QueryFullProcessImageNameW(hProc, 0, path, &len) != 0;

            PROCESS_MEMORY_COUNTERS pmc = {};
            SIZE_T memKB = 0;
            if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                memKB = pmc.WorkingSetSize / 1024;
            }
            CloseHandle(hProc);

            if (!gotPath || !path[0]) continue;

            ProcInfo pi;
            pi.name = pe.szExeFile;
            pi.path = path;
            pi.memKB = memKB;
            g_procList.push_back(pi);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    // Sort by memory, high to low
    std::sort(g_procList.begin(), g_procList.end(),
        [](const ProcInfo& a, const ProcInfo& b) { return a.memKB > b.memKB; });

    // Populate combo
    int selIdx = -1;
    for (size_t i = 0; i < g_procList.size(); i++) {
        wchar_t display[512];
        if (g_procList[i].memKB >= 1024) {
            swprintf(display, 512, L"%s  (%d.%d MB)",
                g_procList[i].name.c_str(),
                (int)(g_procList[i].memKB / 1024),
                (int)((g_procList[i].memKB % 1024) * 10 / 1024));
        } else {
            swprintf(display, 512, L"%s  (%d KB)",
                g_procList[i].name.c_str(), (int)g_procList[i].memKB);
        }

        int idx = (int)SendMessage(g_hCombo, CB_ADDSTRING, 0, (LPARAM)display);
        SendMessage(g_hCombo, CB_SETITEMDATA, idx, (LPARAM)i);

        if (_wcsicmp(g_procList[i].path.c_str(), g_targetPath) == 0)
            selIdx = idx;
    }

    if (selIdx >= 0)
        SendMessage(g_hCombo, CB_SETCURSEL, selIdx, 0);
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

// ---- Apply / Reset ----
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
    SetWindowText(g_hStatus, L"已取消 (当前不在目标程序中)");
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

void OnComboSelect() {
    int sel = (int)SendMessage(g_hCombo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) {
        // User might have selected the "no selection" empty item
        return;
    }

    size_t procIdx = (size_t)SendMessage(g_hCombo, CB_GETITEMDATA, sel, 0);
    if (procIdx >= g_procList.size()) return;

    wcscpy(g_targetPath, g_procList[procIdx].path.c_str());
    AutoSaveConfig();
    SetWindowText(g_hStatus, L"目标程序已设定, 等待该程序切换至前台...");
}

void ClearTarget() {
    g_targetPath[0] = 0;
    SendMessage(g_hCombo, CB_SETCURSEL, (WPARAM)-1, 0);
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

        // Target app dropdown
        CreateWindow(L"STATIC", L"目标程序:",
                     WS_CHILD | WS_VISIBLE,
                     10, 290, 65, 20,
                     hwnd, nullptr, hi, nullptr);

        g_hCombo = CreateWindow(L"COMBOBOX", nullptr,
                     WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
                     80, 288, 300, 200,
                     hwnd, (HMENU)ID_COMBO_TARGET, hi, nullptr);

        CreateWindow(L"BUTTON", L"刷新",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     385, 287, 50, 24,
                     hwnd, (HMENU)ID_BTN_REFRESH, hi, nullptr);

        CreateWindow(L"BUTTON", L"清除",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     440, 287, 40, 24,
                     hwnd, (HMENU)ID_BTN_CLEAR, hi, nullptr);

        // Reset button
        CreateWindow(L"BUTTON", L"恢复默认",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     385, 325, 80, 28,
                     hwnd, (HMENU)ID_BTN_RESET, hi, nullptr);

        // Status bar
        g_hStatus = CreateWindow(L"STATIC", L"就绪 - 拖动滑块调整, 在下拉栏中选择目标程序",
                     WS_CHILD | WS_VISIBLE | SS_LEFT | WS_BORDER,
                     10, 365, 460, 22,
                     hwnd, (HMENU)ID_STATUS, hi, nullptr);

        // Load config and init
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

        RefreshProcessList();
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

        if (notify == CBN_SELCHANGE && ctrlId == ID_COMBO_TARGET) {
            OnComboSelect();
            break;
        }

        switch (ctrlId) {
            case ID_BTN_RESET:
                ResetToDefault();
                break;
            case ID_BTN_REFRESH:
                RefreshProcessList();
                SetWindowText(g_hStatus, L"进程列表已刷新");
                break;
            case ID_BTN_CLEAR:
                ClearTarget();
                break;
        }
        break;
    }

    case WM_CLOSE:
        KillTimer(hwnd, ID_TIMER);
        FreeComboData();
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

    RECT r = { 0, 0, 500, 420 };
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
