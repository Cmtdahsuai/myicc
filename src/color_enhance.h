#pragma once

// Applies a 5x5 color transformation matrix to the full desktop
// via Windows Magnification API (DWM-level, true cross-channel control).
class ColorEnhancer {
public:
    static ColorEnhancer& Instance();

    bool Initialize();
    void Shutdown();
    bool IsReady() const { return m_ready; }

    // saturation: 0.0 = grayscale, 1.0 = normal, 2.0 = double vibrance
    // grayscale: 0.0 = full color, 1.0 = full B&W
    void SetSaturation(float saturation, float grayscale = 0.0f);

    // Reset to identity (no effect)
    void Reset();

private:
    ColorEnhancer() = default;
    void ApplyMatrix(const float matrix[5][5]);

    bool m_ready = false;
    float m_currentSat = 1.0f;
};
