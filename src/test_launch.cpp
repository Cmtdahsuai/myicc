// Try DVC with GPU handles instead of display handles
#include <windows.h>
#include <cstdio>
#include <cstring>

typedef void* (*QI_t)(unsigned int);

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== GPU Handle DVC Test ===\n");

    HMODULE dll = LoadLibraryA("nvapi64.dll");
    auto qi = (QI_t)GetProcAddress(dll, "nvapi_QueryInterface");
    ((int(*)())qi(0x0150E828))();

    // Try to enumerate GPU handles
    // NvAPI_EnumPhysicalGPUs - common ID
    // Let's try known GPU-enum IDs
    unsigned gpuEnumCandidates[] = {
        0xE5AC921F,  // EnumPhysicalGPUs (common)
        0x9ABDD40D,  // EnumNvidiaDisplayHandle (works for display)
    };

    void* gpuHandle = nullptr;
    void* displayHandle = nullptr;

    // Get display handle first
    ((int(*)(unsigned, void**))qi(0x9ABDD40D))(0, &displayHandle);
    printf("Display handle: %p\n", displayHandle);

    // Try to get GPU handle via EnumPhysicalGPUs
    for (auto id : gpuEnumCandidates) {
        auto fn = (int(*)(unsigned, void**))qi(id);
        if (fn) {
            void* h = nullptr;
            int r = fn(0, &h);
            printf("Enum(0x%08X, 0) = %d, handle=%p\n", id, r, h);
            if (r == 0 && h) {
                gpuHandle = h;
                printf("  -> Got GPU handle!\n");
            }
        }
    }

    // Try DVC with both handles
    unsigned dvcIDs[] = { 0x0E45002D, 0x4A82C2B1 };
    void* handles[] = { displayHandle, gpuHandle };
    const char* hnames[] = { "display", "gpu" };

    for (int di = 0; di < 2; di++) {
        unsigned id = dvcIDs[di];
        void* fn = qi(id);
        if (!fn) { printf("DVC 0x%08X: NULL\n", id); continue; }

        for (int hi = 0; hi < 2; hi++) {
            void* h = handles[hi];
            if (!h) continue;

            // Try various struct sizes
            int sizes[] = {4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 64};
            for (int si = 0; si < 11; si++) {
                int sz = sizes[si];
                unsigned buf[32] = {};
                buf[0] = sz | 0x10000;
                buf[1] = (di == 1) ? 80u : 0u;  // set level for Set

                int r = ((int(*)(void*, void*))fn)(h, buf);
                if (r == 0) {
                    printf("DVC 0x%08X + %s handle + sz=%d: OK! data=", id, hnames[hi], sz);
                    for (int j = 0; j < 6; j++) printf(" %u", buf[j]);
                    printf("\n");
                    goto done;
                }
                if (r != -9 && r != -5) {
                    printf("DVC 0x%08X + %s + sz=%d: ret=%d (not -9/-5!)\n", id, hnames[hi], sz, r);
                }
            }
        }
    }
    printf("All combos returned -9 or -5\n");

done:
    // Also try: maybe the DVC functions work on a different type of handle
    // Let's try calling GPU_GetFullName on the display handle - it worked earlier
    printf("\nGPU name from display handle: ");
    char name[256] = {};
    auto gpuNameFn = (int(*)(void*, char*))qi(0xCEEE8E9F);
    if (gpuNameFn) {
        int r = gpuNameFn(displayHandle, name);
        printf("r=%d name='%s'\n", r, name);
    }

    FreeLibrary(dll);
    return 0;
}
