#include "color_controller.h"
#include <cstdio>

ColorController& ColorController::Instance() {
    static ColorController inst;
    return inst;
}

bool ColorController::Initialize() {
    if (m_ready) return true;

    // NVAPI is optional — only needed for DVC (which doesn't work anyway)
    if (!m_nv.Initialize()) {
        fprintf(stderr, "[ColorCtrl] NVAPI init failed — gamma/LUT may be unavailable\n");
    }

    if (!m_enhance.Initialize()) {
        fprintf(stderr, "[ColorCtrl] ColorEnhancer init failed — saturation limited\n");
    }

    SaveOriginalState();
    m_ready = true;
    printf("[ColorCtrl] Ready (saturation=%s, gamma=%s)\n",
           m_enhance.IsReady() ? "DWM matrix" : "LUT",
           m_nv.IsReady() ? "NVAPI+GDI" : "GDI only");
    return true;
}

void ColorController::Shutdown() {
    if (m_hasOriginalState) RestoreOriginalState();
    m_enhance.Shutdown();
    m_nv.Shutdown();
    m_ready = false;
}

bool ColorController::IsReady() const { return m_ready; }

bool ColorController::SaveOriginalState() {
    // GDI gamma ramp works on any GPU, not just NVIDIA
    if (m_nv.GetGDIGammaRamp(m_originalRamp)) {
        m_hasOriginalState = true;
        return true;
    }
    return false;
}

bool ColorController::RestoreOriginalState() {
    if (!m_hasOriginalState) return false;
    m_enhance.Reset();
    return m_nv.SetGDIGammaRamp(m_originalRamp);
}

bool ColorController::ApplySettings(const ColorParams& params) {
    m_current = params;
    return SetGammaRamp(params);
}

bool ColorController::SetGammaRamp(const ColorParams& params) {

    // Saturation handled by color matrix
    if (m_enhance.IsReady()) {
        float sat = params.saturation / 50.0f;  // 0..2 (1=neutral at slider 50)
        m_enhance.SetSaturation(sat);
    }

    GDI_GAMMA_RAMP ramp;
    if (params.gamma == 50 && params.brightness == 50 &&
        params.contrast == 50 && params.temperature == 50 &&
        params.saturation == 50) {
        GammaEngine::GenerateNeutral(ramp);
        if (m_enhance.IsReady()) m_enhance.Reset();
    } else {
        // Build ramp with saturation disabled (handled by matrix)
        ColorParams lutParams = params;
        lutParams.saturation = 50;  // neutral - no LUT saturation
        GammaEngine::GenerateRamp(lutParams, ramp);
    }
    return m_nv.SetGDIGammaRamp(ramp);
}
