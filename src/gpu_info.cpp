#include <windows.h>
#include <cstdio>

int main() {
    HKEY hk;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000",
        0, KEY_READ, &hk) == 0) {
        char chip[256] = {}, adapter[256] = {}, mem[32] = {};
        DWORD sz = sizeof(chip);
        RegQueryValueExA(hk, "HardwareInformation.ChipType", nullptr, nullptr, (BYTE*)chip, &sz);
        sz = sizeof(adapter);
        RegQueryValueExA(hk, "HardwareInformation.AdapterString", nullptr, nullptr, (BYTE*)adapter, &sz);
        sz = sizeof(mem);
        DWORD memVal;
        RegQueryValueExA(hk, "HardwareInformation.MemorySize", nullptr, nullptr, (BYTE*)&memVal, &sz);

        printf("GPU Chip: %s\n", chip);
        printf("Adapter:  %s\n", adapter);
        printf("VRAM:     %.1f GB\n", memVal / (1024.0 * 1024.0 * 1024.0));
        RegCloseKey(hk);
    }

    // Also check Optimus status via EnumDisplayDevices
    DISPLAY_DEVICEA dd = { sizeof(dd) };
    for (int i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); i++) {
        printf("Display %d: %s (flags=0x%X)\n", i, dd.DeviceString, dd.StateFlags);
    }

    return 0;
}
