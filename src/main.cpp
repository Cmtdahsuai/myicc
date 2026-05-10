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
#include <shellapi.h>
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
#define ID_BTN_DROPDOWN 2007
#define ID_LISTBOX      2008
#define ID_STATUS       3001
#define ID_TIMER        1

HINSTANCE g_hInst;
HWND g_hWnd = nullptr;
HWND g_hSliders[5] = {};
HWND g_hEdits[5] = {};
HWND g_hStatus;
HWND g_hBtnDrop;
HWND g_hListBox;
HWND g_hListDlg;  // popup window hosting the listbox
HFONT g_hFont = nullptr;

ColorParams g_params;
bool g_dirty = false;
bool g_updating = false;
bool g_effectActive = false;
wchar_t g_targetPath[MAX_PATH] = {};
wchar_t g_targetDisplay[256] = L"(全局模式 - 不限制)";
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
            wchar_t* lastSlash = wcsrchr(g_targetPath, L'\\');
            wcscpy(g_targetDisplay, lastSlash ? lastSlash + 1 : g_targetPath);
        }
    }
    fclose(f);
}

// ---- Process enumeration ----
struct ProcInfo {
    std::wstring name;
    std::wstring path;
    SIZE_T memKB;
    HICON hIcon;
};
std::vector<ProcInfo> g_procList;

void ApplyToGPU();
void CloseListPopup();

WNDPROC g_oldListDlgProc = nullptr;
LRESULT CALLBACK ListDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND)
        return SendMessageW(g_hWnd, WM_COMMAND, wParam, lParam);
    if (msg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE)
        CloseListPopup();

    if (msg == WM_MEASUREITEM) {
        LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
        mis->itemHeight = 22;
        return TRUE;
    }

    if (msg == WM_DRAWITEM) {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->itemID == (UINT)-1) return TRUE;

        wchar_t text[512];
        SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);

        HICON hIcon = nullptr;
        INT_PTR data = (INT_PTR)SendMessageW(dis->hwndItem, LB_GETITEMDATA, dis->itemID, 0);
        if (data != -1 && data < (INT_PTR)g_procList.size())
            hIcon = g_procList[(size_t)data].hIcon;

        bool sel = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF bgClr = GetSysColor(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW);
        HBRUSH bg = GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW);
        FillRect(dis->hDC, &dis->rcItem, bg);
        SetBkColor(dis->hDC, bgClr);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));

        if (hIcon)
            DrawIconEx(dis->hDC, dis->rcItem.left + 3, dis->rcItem.top + 3, hIcon, 16, 16, 0, nullptr, DI_NORMAL);

        RECT tr = dis->rcItem;
        tr.left += 24;
        DrawTextW(dis->hDC, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (dis->itemState & ODS_FOCUS)
            DrawFocusRect(dis->hDC, &dis->rcItem);
        return TRUE;
    }

    return CallWindowProcW(g_oldListDlgProc, hwnd, msg, wParam, lParam);
}

void CloseListPopup() {
    if (g_hListDlg) {
        DestroyWindow(g_hListDlg);
        g_hListDlg = nullptr;
        g_hListBox = nullptr;
        // Note: icons are cleaned in RefreshProcessList / WM_DESTROY
    }
}

void RefreshProcessList() {
    CloseListPopup();
    // Clean up old icons
    for (auto& p : g_procList) { if (p.hIcon) DestroyIcon(p.hIcon); }
    g_procList.clear();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == 0) continue;

            HANDLE hProc = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
            if (!hProc) continue;

            wchar_t path[MAX_PATH] = {};
            DWORD len = MAX_PATH;
            bool gotPath = QueryFullProcessImageNameW(hProc, 0, path, &len) != 0;

            PROCESS_MEMORY_COUNTERS_EX pmc = {};
            SIZE_T memKB = 0;
            if (GetProcessMemoryInfo(hProc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                memKB = pmc.WorkingSetSize / 1024;
            }
            CloseHandle(hProc);
            if (!gotPath || !path[0]) continue;

            ProcInfo pi;
            pi.name = pe.szExeFile;
            pi.path = path;
            pi.memKB = memKB;
            SHFILEINFOW sfi = {};
            pi.hIcon = nullptr;
            if (SHGetFileInfoW(path, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON))
                pi.hIcon = sfi.hIcon;
            g_procList.push_back(pi);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    std::sort(g_procList.begin(), g_procList.end(),
        [](const ProcInfo& a, const ProcInfo& b) { return a.memKB > b.memKB; });
}

void ShowListPopup() {
    if (g_hListDlg) { CloseListPopup(); return; }

    // Refresh list each time we open
    RefreshProcessList();

    RECT btnRect;
    GetWindowRect(g_hBtnDrop, &btnRect);

    int itemHeight = 22;
    int listH = (int)g_procList.size() * itemHeight + itemHeight;  // +1 for global mode
    if (listH > 400) listH = 400;

    g_hListDlg = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"STATIC", nullptr,
        WS_POPUP | WS_BORDER,
        btnRect.left, btnRect.bottom,
        440, listH,
        g_hWnd, nullptr, g_hInst, nullptr);
    if (!g_hListDlg) return;

    // Subclass popup to forward WM_COMMAND from listbox to main window
    g_oldListDlgProc = (WNDPROC)SetWindowLongPtrW(g_hListDlg, GWLP_WNDPROC, (LONG_PTR)ListDlgProc);

    SendMessageW(g_hListDlg, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    g_hListBox = CreateWindowExW(0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
        0, 0, 440, listH,
        g_hListDlg, (HMENU)ID_LISTBOX, g_hInst, nullptr);
    SendMessageW(g_hListBox, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Global mode option
    int globalIdx = (int)SendMessageW(g_hListBox, LB_ADDSTRING, 0, (LPARAM)L"(全局模式 - 不限制)");
    SendMessageW(g_hListBox, LB_SETITEMDATA, globalIdx, (LPARAM)-1);

    int selIdx = globalIdx;
    size_t showCount = g_procList.size() < 20 ? g_procList.size() : 20;
    for (size_t i = 0; i < showCount; i++) {
        wchar_t display[512];
        double mem = (double)g_procList[i].memKB;
        if (mem >= 1048576.0)
            swprintf(display, 512, L"%ls  (%.1f GB)", g_procList[i].name.c_str(), mem / 1048576.0);
        else if (mem >= 1024.0)
            swprintf(display, 512, L"%ls  (%.1f MB)", g_procList[i].name.c_str(), mem / 1024.0);
        else
            swprintf(display, 512, L"%ls  (%d KB)", g_procList[i].name.c_str(), (int)mem);

        int idx = (int)SendMessageW(g_hListBox, LB_ADDSTRING, 0, (LPARAM)display);
        SendMessageW(g_hListBox, LB_SETITEMDATA, idx, (LPARAM)i);

        if (g_targetPath[0] && _wcsicmp(g_procList[i].path.c_str(), g_targetPath) == 0)
            selIdx = idx;
    }

    SendMessageW(g_hListBox, LB_SETCURSEL, selIdx, 0);
    ShowWindow(g_hListDlg, SW_SHOW);
    SetFocus(g_hListBox);
}

void OnListSelect(int sel) {
    if (sel == LB_ERR) return;
    INT_PTR data = (INT_PTR)SendMessageW(g_hListBox, LB_GETITEMDATA, sel, 0);

    // Close popup first to avoid re-entrancy with timer/activate
    CloseListPopup();

    if (data == -1) {
        g_targetPath[0] = 0;
        wcscpy(g_targetDisplay, L"(全局模式 - 不限制)");
        ApplyToGPU();
    } else {
        size_t procIdx = (size_t)data;
        if (procIdx >= g_procList.size()) return;
        wcscpy(g_targetPath, g_procList[procIdx].path.c_str());
        wcscpy(g_targetDisplay, g_procList[procIdx].name.c_str());
        SetWindowText(g_hStatus, L"已设定目标程序, 等待该程序进入前台...");
    }

    SetWindowText(g_hBtnDrop, g_targetDisplay);
    AutoSaveConfig();
}

void ClearTarget() {
    g_targetPath[0] = 0;
    wcscpy(g_targetDisplay, L"(全局模式 - 不限制)");
    SetWindowText(g_hBtnDrop, g_targetDisplay);
    CloseListPopup();
    AutoSaveConfig();
    ApplyToGPU();
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
    if (g_effectActive || !g_targetPath[0]) ApplyToGPU();
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

        g_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        const wchar_t* names[] = { L"饱和度:", L"亮度:", L"对比度:", L"色温:", L"伽马:" };
        int sliderIds[] = { ID_SATURATION, ID_BRIGHTNESS, ID_CONTRAST, ID_TEMPERATURE, ID_GAMMA };
        int editIds[]   = { ID_EDIT_SAT, ID_EDIT_BRI, ID_EDIT_CON, ID_EDIT_TMP, ID_EDIT_GAM };

        for (int i = 0; i < 5; i++) {
            HWND lbl = CreateWindow(L"STATIC", names[i],
                         WS_CHILD | WS_VISIBLE,
                         10, 12 + i * 50, 60, 20,
                         hwnd, nullptr, hi, nullptr);
            SendMessage(lbl, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            g_hSliders[i] = CreateWindow(TRACKBAR_CLASS, nullptr,
                         WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                         75, 30 + i * 50, 310, 28,
                         hwnd, (HMENU)(UINT_PTR)sliderIds[i], hi, nullptr);
            SendMessage(g_hSliders[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessage(g_hSliders[i], TBM_SETPOS, TRUE, 50);

            g_hEdits[i] = CreateWindow(L"EDIT", L"50",
                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
                         395, 30 + i * 50, 45, 22,
                         hwnd, (HMENU)(UINT_PTR)editIds[i], hi, nullptr);
            SendMessage(g_hEdits[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);
        }

        // Target app section
        HWND lbl = CreateWindow(L"STATIC", L"目标程序:",
                     WS_CHILD | WS_VISIBLE,
                     10, 265, 70, 20,
                     hwnd, nullptr, hi, nullptr);
        SendMessage(lbl, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        // Dropdown button + refresh on same row
        g_hBtnDrop = CreateWindow(L"BUTTON", g_targetDisplay,
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_LEFT,
                     80, 261, 335, 24,
                     hwnd, (HMENU)ID_BTN_DROPDOWN, hi, nullptr);
        SendMessage(g_hBtnDrop, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        CreateWindow(L"BUTTON", L"刷新",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     420, 260, 45, 26,
                     hwnd, (HMENU)ID_BTN_REFRESH, hi, nullptr);

        CreateWindow(L"BUTTON", L"清除",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     10, 296, 52, 24,
                     hwnd, (HMENU)ID_BTN_CLEAR, hi, nullptr);
        CreateWindow(L"BUTTON", L"恢复默认",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     68, 296, 80, 24,
                     hwnd, (HMENU)ID_BTN_RESET, hi, nullptr);

        g_hStatus = CreateWindow(L"STATIC", L"就绪 - 拖动滑块调整色彩, 点击目标程序选择",
                     WS_CHILD | WS_VISIBLE | SS_LEFT | WS_BORDER,
                     10, 335, 460, 22,
                     hwnd, (HMENU)ID_STATUS, hi, nullptr);
        SendMessage(g_hStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        LoadConfig();
        SetWindowText(g_hBtnDrop, g_targetDisplay);

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

        SetTimer(hwnd, ID_TIMER, 500, nullptr);
        ApplyToGPU();
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_TIMER: {
        static bool wasForeground = true;
        bool hasNoTarget = !g_targetPath[0];
        bool isForeground = hasNoTarget || IsTargetForeground();

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
        if (g_effectActive || !g_targetPath[0]) ApplyToGPU();
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

        if (notify == LBN_SELCHANGE && ctrlId == ID_LISTBOX) {
            int sel = (int)SendMessageW(g_hListBox, LB_GETCURSEL, 0, 0);
            OnListSelect(sel);
            break;
        }

        switch (ctrlId) {
            case ID_BTN_DROPDOWN:
                ShowListPopup();
                break;
            case ID_BTN_RESET:
                ResetToDefault();
                break;
            case ID_BTN_REFRESH:
                CloseListPopup();
                RefreshProcessList();
                SetWindowText(g_hStatus, L"进程列表已刷新 (请重新点击目标程序按钮)");
                break;
            case ID_BTN_CLEAR:
                ClearTarget();
                break;
        }
        break;
    }

    case WM_CLOSE:
        KillTimer(hwnd, ID_TIMER);
        CloseListPopup();
        ColorController::Instance().Shutdown();
        // g_hFont is a stock object, no need to delete
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

    RECT r = { 0, 0, 500, 400 };
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
