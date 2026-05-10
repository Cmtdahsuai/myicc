#pragma once
#include "nvapi_wrapper.h"

// Parameters (mirrors 薯条ICC TOML config ranges)
struct ColorParams {
    int saturation  = 50;    // 0-100  (digital vibrance)
    int grayscale   = 0;     // 0-100  (0=full color, 100=full B&W)
    int brightness  = 50;    // 0-100  (50 = neutral)
    int contrast    = 50;    // 0-100  (50 = neutral)
    int temperature = 50;    // 0-100  (50 = 6500K neutral)
    int gamma       = 50;    // 0-100  (50 = gamma 1.0 neutral)
};

class GammaEngine {
public:
    // Generate a neutral (identity) gamma ramp
    static void GenerateNeutral(GDI_GAMMA_RAMP& ramp);

    // Generate ramp from ColorParams
    static void GenerateRamp(const ColorParams& params, GDI_GAMMA_RAMP& ramp);

    // Color temperature to RGB gains (Kelvin input, outputs multipliers around 1.0)
    static void TemperatureToRGB(float kelvin, float& rGain, float& gGain, float& bGain);

    // Map slider value (0-100) to gamma curve value (0.5 - 3.0)
    static float SliderToGamma(int slider);

    // Map slider value (0-100, center 50) to brightness offset
    static float SliderToBrightness(int slider);

    // Map slider value (0-100, center 50) to contrast multiplier
    static float SliderToContrast(int slider);

    // Map slider value (0-100, center 50) to Kelvin temperature (3000K - 10000K)
    static float SliderToKelvin(int slider);

    // Map saturation slider to per-channel gain modulation
    static float SliderToSaturationGain(int slider);

private:
    static void ApplyGamma(GDI_GAMMA_RAMP& ramp, float gamma, float brightness, float contrast,
                           float rGain, float gGain, float bGain, float satGain);
};
