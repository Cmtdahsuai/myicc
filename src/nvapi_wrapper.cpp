#include "nvapi_wrapper.h"
#include <cstring>
#include <cstdio>

NvAPIWrapper& NvAPIWrapper::Instance() {
    static NvAPIWrapper inst;
    return inst;
}

NvAPIWrapper::~NvAPIWrapper() {
    Shutdown();
}

bool NvAPIWrapper::Initialize() {
    if (m_ready) return true;

    m_dll = LoadLibraryA("nvapi64.dll");
    if (!m_dll) {
        fprintf(stderr, "[NvAPI] Failed to load nvapi64.dll\n");
        return false;
    }

    m_queryInterface = (NvAPI_QueryInterface_t)GetProcAddress(m_dll, "nvapi_QueryInterface");
    if (!m_queryInterface) {
        fprintf(stderr, "[NvAPI] Failed to get nvapi_QueryInterface\n");
        FreeLibrary(m_dll);
        m_dll = nullptr;
        return false;
    }

    // Initialize via hardcoded ID
    auto initFunc = (NvAPI_Initialize_t)ResolveById(NVAPI_ID_Initialize);
    if (!initFunc) {
        fprintf(stderr, "[NvAPI] Failed to resolve NvAPI_Initialize (ID=0x%08X)\n", NVAPI_ID_Initialize);
        FreeLibrary(m_dll);
        m_dll = nullptr;
        return false;
    }

    NvAPI_Status initRet = initFunc();
    if (initRet != NVAPI_OK) {
        fprintf(stderr, "[NvAPI] NvAPI_Initialize returned %d\n", initRet);
        FreeLibrary(m_dll);
        m_dll = nullptr;
        return false;
    }

    // Resolve all function pointers
    pEnumDisplay     = (NvAPI_EnumNvidiaDisplayHandle_t)ResolveById(NVAPI_ID_EnumNvidiaDisplayHandle);
    pGetAssocDisplay = (NvAPI_GetAssociatedNvidiaDisplayHandle_t)ResolveById(NVAPI_ID_GetAssociatedNvidiaDisplayHandle);
    pGetDVCEx        = (NvAPI_GetDVCInfoEx_t)ResolveById(NVAPI_ID_GetDVCInfoEx);
    pSetDVCEx        = (NvAPI_SetDVCLevelEx_t)ResolveById(NVAPI_ID_SetDVCLevelEx);

    // Enumerate displays
    if (pEnumDisplay) {
        for (NvU32 i = 0; i < 16; i++) {
            NvDisplayHandle h = nullptr;
            if (pEnumDisplay(i, &h) != NVAPI_OK || !h) break;

            DisplayInfo info;
            info.handle = h;
            info.isPrimary = (i == 0);
            m_displays.push_back(info);
        }
    }

    m_ready = !m_displays.empty();
    if (m_ready) {
        printf("[NvAPI] Initialized OK. %zu display(s). DVC Ex: %s. GDI Gamma: %s\n",
               m_displays.size(),
               (pGetDVCEx && pSetDVCEx) ? "YES" : "NO",
               "YES");
    }
    return m_ready;
}

void NvAPIWrapper::Shutdown() {
    if (m_dll) {
        FreeLibrary(m_dll);
        m_dll = nullptr;
    }
    m_queryInterface = nullptr;
    m_ready = false;
    m_displays.clear();
}

NvDisplayHandle NvAPIWrapper::GetPrimaryHandle() const {
    if (m_displays.empty()) return nullptr;
    for (auto& d : m_displays)
        if (d.isPrimary) return d.handle;
    return m_displays[0].handle;
}

void* NvAPIWrapper::ResolveById(NvU32 id) {
    if (!m_queryInterface) return nullptr;
    void* fn = m_queryInterface(id);
    if (!fn) {
        fprintf(stderr, "[NvAPI] ResolveById(0x%08X) -> NULL\n", id);
    }
    return fn;
}

// ---- Digital Vibrance via Ex DVC ----
bool NvAPIWrapper::GetDigitalVibrance(NvDisplayHandle h, NvU32& level) {
    if (!pGetDVCEx) return false;
    NV_DVC_INFO_EX info = {};
    info.version = sizeof(info) | 0x10000;
    NvAPI_Status ret = pGetDVCEx(h, &info);
    if (ret == NVAPI_OK) {
        level = info.currentLevel;
        return true;
    }
    fprintf(stderr, "[NvAPI] GetDVCInfoEx returned %d\n", ret);
    return false;
}

bool NvAPIWrapper::SetDigitalVibrance(NvDisplayHandle h, NvU32 level) {
    if (!pSetDVCEx) return false;
    NvAPI_Status ret = pSetDVCEx(h, level);
    if (ret != NVAPI_OK) {
        fprintf(stderr, "[NvAPI] SetDVCLevelEx(%u) returned %d\n", level, ret);
        return false;
    }
    return true;
}

// ---- GDI Gamma Ramp ----
bool NvAPIWrapper::GetGDIGammaRamp(GDI_GAMMA_RAMP& ramp) {
    HDC hdc = GetDC(nullptr);
    if (!hdc) return false;
    BOOL ok = GetDeviceGammaRamp(hdc, &ramp);
    ReleaseDC(nullptr, hdc);
    return ok != FALSE;
}

bool NvAPIWrapper::SetGDIGammaRamp(const GDI_GAMMA_RAMP& ramp) {
    HDC hdc = GetDC(nullptr);
    if (!hdc) return false;
    BOOL ok = SetDeviceGammaRamp(hdc, (LPVOID)&ramp);
    ReleaseDC(nullptr, hdc);
    return ok != FALSE;
}
