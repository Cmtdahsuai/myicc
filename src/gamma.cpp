#include "gamma.h"
#include <cmath>
#include <algorithm>

void GammaEngine::GenerateNeutral(GDI_GAMMA_RAMP& ramp) {
    for (int i = 0; i < 256; i++) {
        WORD val = (WORD)(i * 256 + i);  // map 0-255 → 0-65535 linearly
        ramp.Red[i]   = val;
        ramp.Green[i] = val;
        ramp.Blue[i]  = val;
    }
}

void GammaEngine::GenerateRamp(const ColorParams& p, GDI_GAMMA_RAMP& ramp) {
    float gamma    = SliderToGamma(p.gamma);
    float bright   = SliderToBrightness(p.brightness);
    float contrast = SliderToContrast(p.contrast);
    float kelvin   = SliderToKelvin(p.temperature);
    float satGain  = SliderToSaturationGain(p.saturation);
    float rGain, gGain, bGain;
    TemperatureToRGB(kelvin, rGain, gGain, bGain);

    ApplyGamma(ramp, gamma, bright, contrast, rGain, gGain, bGain, satGain);
}

// ---- Slider Mappings ----
float GammaEngine::SliderToGamma(int slider) {
    float t = (slider - 50) / 50.0f;
    if (t >= 0)
        return 1.0f + t * 2.0f;           // 1.0 - 3.0
    else
        return 1.0f / (1.0f - t * 2.0f);  // 0.5 - 1.0
}

float GammaEngine::SliderToBrightness(int slider) {
    return (slider - 50) / 50.0f * 0.30f;
}

float GammaEngine::SliderToContrast(int slider) {
    float t = (slider - 50) / 50.0f;
    if (t >= 0)
        return 1.0f + t * 2.0f;
    else
        return 1.0f / (1.0f - t * 2.0f);
}

float GammaEngine::SliderToKelvin(int slider) {
    if (slider <= 50)
        return 1000.0f + slider * 110.0f;        // 1000K - 6500K
    else
        return 6500.0f + (slider - 50) * 170.0f; // 6500K - 15000K
}

// ---- Color Temperature to RGB (Tanner Helland algorithm) ----
void GammaEngine::TemperatureToRGB(float kelvin, float& rGain, float& gGain, float& bGain) {
    float temp = kelvin / 100.0f;
    float r, g, b;

    if (temp <= 66)
        r = 255.0f;
    else
        r = 329.698727446f * std::pow(temp - 60.0f, -0.1332047592f);

    if (temp <= 66)
        g = 99.4708025861f * std::log(temp) - 161.1195681661f;
    else
        g = 288.1221695283f * std::pow(temp - 60.0f, -0.0755148492f);

    if (temp >= 66)
        b = 255.0f;
    else if (temp <= 19)
        b = 0.0f;
    else
        b = 138.5177312231f * std::log(temp - 10.0f) - 305.0447927307f;

    r = std::clamp(r, 0.0f, 255.0f);
    g = std::clamp(g, 0.0f, 255.0f);
    b = std::clamp(b, 0.0f, 255.0f);

    float gRef = std::max(g, 1.0f);
    rGain = r / gRef;
    gGain = g / gRef;
    bGain = b / gRef;
}

float GammaEngine::SliderToSaturationGain(int slider) {
    // 0=grayscale, 50=neutral, 100=extreme vibrance
    float t = (slider - 50) / 50.0f;  // -1.0 to 1.0
    if (t >= 0)
        return 1.0f + t * 4.0f;       // 1.0 to 5.0 (very aggressive)
    else
        return 1.0f + t * 1.0f;       // 0.0 to 1.0 (full grayscale at 0)
}

// ---- Apply all adjustments to GDI gamma ramp ----
void GammaEngine::ApplyGamma(GDI_GAMMA_RAMP& ramp, float gamma, float brightness,
                              float contrast, float rGain, float gGain, float bGain,
                              float satGain) {
    float maxVal = 65535.0f;

    for (int i = 0; i < 256; i++) {
        float in = (float)i / 255.0f;

        in = std::clamp(in + brightness, 0.0f, 1.0f);

        if (contrast != 1.0f) {
            in = (in - 0.5f) * contrast + 0.5f;
            in = std::clamp(in, 0.0f, 1.0f);
        }

        float out = std::pow(in, 1.0f / gamma);

        float rVal = std::clamp(out * rGain, 0.0f, 1.0f);
        float gVal = std::clamp(out * gGain, 0.0f, 1.0f);
        float bVal = std::clamp(out * bGain, 0.0f, 1.0f);

        // Saturation: expand colors around their luminance
        // This works within the 1D LUT by making each channel deviate from the
        // neutral gray value proportionally — like per-channel contrast boost
        if (satGain != 1.0f) {
            // Compute neutral (luminance) value for this intensity
            float gray = 0.299f * rVal + 0.587f * gVal + 0.114f * bVal;

            // Push each channel away from (or toward) gray based on satGain
            rVal = std::clamp(gray + satGain * (rVal - gray), 0.0f, 1.0f);
            gVal = std::clamp(gray + satGain * (gVal - gray), 0.0f, 1.0f);
            bVal = std::clamp(gray + satGain * (bVal - gray), 0.0f, 1.0f);
        }

        ramp.Red[i]   = (WORD)(rVal * maxVal);
        ramp.Green[i] = (WORD)(gVal * maxVal);
        ramp.Blue[i]  = (WORD)(bVal * maxVal);
    }
}
