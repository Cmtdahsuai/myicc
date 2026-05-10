#pragma once

// Takes control of Windows Color System to prevent DWM from resetting gamma ramp
class WcsControl {
public:
    static WcsControl& Instance();

    // Lock display calibration so our gamma ramp persists
    bool LockCalibration();

    // Release back to Windows
    void UnlockCalibration();

    bool IsLocked() const { return m_locked; }

private:
    WcsControl() = default;
    bool m_locked = false;
    void* m_mscmsDll = nullptr;
};
