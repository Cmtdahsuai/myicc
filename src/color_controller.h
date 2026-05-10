#pragma once
#include "nvapi_wrapper.h"
#include "gamma.h"
#include "color_enhance.h"

class ColorController {
public:
    static ColorController& Instance();

    bool Initialize();
    void Shutdown();
    bool IsReady() const;
    bool ApplySettings(const ColorParams& params);
    bool SetGammaRamp(const ColorParams& params);

    const ColorParams& GetCurrentParams() const { return m_current; }
    bool SaveOriginalState();
    bool RestoreOriginalState();

private:
    ColorController() : m_nv(NvAPIWrapper::Instance()), m_enhance(ColorEnhancer::Instance()) {}
    NvAPIWrapper& m_nv;
    ColorEnhancer&  m_enhance;
    ColorParams     m_current;
    GDI_GAMMA_RAMP  m_originalRamp;
    bool            m_hasOriginalState = false;
    bool            m_ready = false;
};
