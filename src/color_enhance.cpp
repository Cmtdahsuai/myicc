#include "color_enhance.h"
#include <windows.h>
#include <cstring>
#include <cstdio>

// Windows Magnification API types and function pointers (dynamically loaded)
typedef struct {
    float transform[5][5];
} MAGCOLOREFFECT;

typedef BOOL (WINAPI *MagInitialize_t)();
typedef BOOL (WINAPI *MagUninitialize_t)();
typedef BOOL (WINAPI *MagSetFullscreenColorEffect_t)(const MAGCOLOREFFECT*);

static HMODULE s_magDll = nullptr;
static MagInitialize_t              pMagInit = nullptr;
static MagUninitialize_t            pMagUninit = nullptr;
static MagSetFullscreenColorEffect_t pMagSetEffect = nullptr;

ColorEnhancer& ColorEnhancer::Instance() {
    static ColorEnhancer inst;
    return inst;
}

bool ColorEnhancer::Initialize() {
    if (m_ready) return true;

    s_magDll = LoadLibraryA("magnification.dll");
    if (!s_magDll) {
        fprintf(stderr, "[ColorEnhancer] magnification.dll not found\n");
        return false;
    }

    pMagInit     = (MagInitialize_t)GetProcAddress(s_magDll, "MagInitialize");
    pMagUninit   = (MagUninitialize_t)GetProcAddress(s_magDll, "MagUninitialize");
    pMagSetEffect = (MagSetFullscreenColorEffect_t)GetProcAddress(s_magDll, "MagSetFullscreenColorEffect");

    if (!pMagInit || !pMagUninit || !pMagSetEffect) {
        fprintf(stderr, "[ColorEnhancer] Failed to resolve Magnification API\n");
        FreeLibrary(s_magDll);
        s_magDll = nullptr;
        return false;
    }

    if (!pMagInit()) {
        fprintf(stderr, "[ColorEnhancer] MagInitialize failed\n");
        FreeLibrary(s_magDll);
        s_magDll = nullptr;
        return false;
    }

    m_ready = true;
    printf("[ColorEnhancer] Ready - fullscreen color matrix via DWM\n");
    return true;
}

void ColorEnhancer::Shutdown() {
    if (m_ready) {
        Reset();
        if (pMagUninit) pMagUninit();
        m_ready = false;
    }
    if (s_magDll) {
        FreeLibrary(s_magDll);
        s_magDll = nullptr;
    }
    pMagInit = nullptr;
    pMagUninit = nullptr;
    pMagSetEffect = nullptr;
}

void ColorEnhancer::Reset() {
    float identity[5][5] = {
        {1, 0, 0, 0, 0},
        {0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0},
        {0, 0, 0, 0, 1}
    };
    ApplyMatrix(identity);
    m_currentSat = 1.0f;
}

void ColorEnhancer::SetSaturation(float sat, float gray) {
    if (!m_ready) return;

    // Effective saturation = saturation * (1 - grayscale)
    float effectiveSat = sat * (1.0f - gray);
    m_currentSat = effectiveSat;

    const float rLum = 0.299f;
    const float gLum = 0.587f;
    const float bLum = 0.114f;
    float t = 1.0f - effectiveSat;

    // out = gray + sat * (in - gray)  where gray = luminance
    // Each row = one input channel; columns = output channels.
    // Off-diagonal terms use the INPUT row's luminance weight (not the output's).
    float matrix[5][5] = {
        {rLum + sat * (1.0f - rLum),  rLum * t,                     rLum * t,                      0, 0},
        {gLum * t,                     gLum + sat * (1.0f - gLum),  gLum * t,                      0, 0},
        {bLum * t,                     bLum * t,                     bLum + sat * (1.0f - bLum),   0, 0},
        {0,                            0,                            0,                            1, 0},
        {0,                            0,                            0,                            0, 1}
    };

    ApplyMatrix(matrix);
}

void ColorEnhancer::ApplyMatrix(const float matrix[5][5]) {
    if (!pMagSetEffect) return;

    MAGCOLOREFFECT effect;
    std::memcpy(effect.transform, matrix, sizeof(effect.transform));

    if (!pMagSetEffect(&effect)) {
        fprintf(stderr, "[ColorEnhancer] MagSetFullscreenColorEffect failed: %lu\n", GetLastError());
    }
}
