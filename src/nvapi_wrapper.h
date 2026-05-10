#pragma once
#include <windows.h>

// Use only the type-definition header from the official NVAPI SDK.
// The full <nvapi.h> uses MSVC SAL annotations (__in, __out, etc.)
// that collide with libstdc++ internal identifiers under MinGW/GCC.
// Since we use dynamic loading (LoadLibrary + QueryInterface), we
// only need the type definitions, not the function declarations.
#include <nvapi_lite_common.h>

#include <vector>
#include <string>

// ---- Extensions not in public NVAPI SDK ----

// Hardcoded NVAPI function IDs for QueryInterface
#define NVAPI_ID_Initialize                      0x0150E828
#define NVAPI_ID_EnumNvidiaDisplayHandle         0x9ABDD40D
#define NVAPI_ID_GetAssociatedNvidiaDisplayHandle 0x35C29134
#define NVAPI_ID_GPU_GetFullName                 0xCEEE8E9F
#define NVAPI_ID_GetDVCInfoEx                    0x0E45002D
#define NVAPI_ID_SetDVCLevelEx                   0x4A82C2B1

// NvAPI function pointer typedefs
typedef void* (*NvAPI_QueryInterface_t)(unsigned int id);
typedef NvAPI_Status (*NvAPI_Initialize_t)();
typedef NvAPI_Status (*NvAPI_EnumNvidiaDisplayHandle_t)(NvU32 index, NvDisplayHandle* handle);
typedef NvAPI_Status (*NvAPI_GetAssociatedNvidiaDisplayHandle_t)(const char* name, NvDisplayHandle* handle);
typedef NvAPI_Status (*NvAPI_GetDVCInfoEx_t)(NvDisplayHandle h, void* info);
typedef NvAPI_Status (*NvAPI_SetDVCLevelEx_t)(NvDisplayHandle h, NvU32 level);

// Digital Vibrance Control (Ex version) - not in public SDK
typedef struct {
    NvU32 version;
    NvU32 currentLevel;
    NvU32 defaultLevel;
    NvU32 minLevel;
    NvU32 maxLevel;
} NV_DVC_INFO_EX;

// GDI Gamma Ramp (Windows GDI, not NVAPI)
typedef struct {
    WORD  Red[256];
    WORD  Green[256];
    WORD  Blue[256];
} GDI_GAMMA_RAMP;

struct DisplayInfo {
    NvDisplayHandle handle;
    std::string     gpuName;
    bool            isPrimary;
};

class NvAPIWrapper {
public:
    static NvAPIWrapper& Instance();

    bool Initialize();
    void Shutdown();
    bool IsReady() const { return m_ready; }

    const std::vector<DisplayInfo>& GetDisplays() const { return m_displays; }
    NvDisplayHandle GetPrimaryHandle() const;

    // Digital Vibrance (0-100) via Ex DVC functions
    bool GetDigitalVibrance(NvDisplayHandle h, NvU32& level);
    bool SetDigitalVibrance(NvDisplayHandle h, NvU32 level);

    // GDI Gamma Ramp (Windows fallback)
    bool GetGDIGammaRamp(GDI_GAMMA_RAMP& ramp);
    bool SetGDIGammaRamp(const GDI_GAMMA_RAMP& ramp);

private:
    NvAPIWrapper() = default;
    ~NvAPIWrapper();
    NvAPIWrapper(const NvAPIWrapper&) = delete;
    NvAPIWrapper& operator=(const NvAPIWrapper&) = delete;

    void* ResolveById(NvU32 id);

    HMODULE m_dll = nullptr;
    NvAPI_QueryInterface_t m_queryInterface = nullptr;
    bool m_ready = false;
    std::vector<DisplayInfo> m_displays;

    // Cached function pointers
    NvAPI_EnumNvidiaDisplayHandle_t             pEnumDisplay = nullptr;
    NvAPI_GetAssociatedNvidiaDisplayHandle_t    pGetAssocDisplay = nullptr;
    NvAPI_GetDVCInfoEx_t                        pGetDVCEx = nullptr;
    NvAPI_SetDVCLevelEx_t                       pSetDVCEx = nullptr;
};
