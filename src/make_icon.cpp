// Generates fries.ico with 32x32 and 16x16 sizes
#include <windows.h>
#include <cstdio>
#include <vector>

void WriteLE32(FILE* f, int v) { fwrite(&v, 1, 4, f); }
void WriteLE16(FILE* f, short v) { fwrite(&v, 1, 2, f); }

int main() {
    int sizes[] = {32, 16};
    int count = 2;

    // Pre-render pixel data
    std::vector<unsigned char> pixels[2];
    for (int s = 0; s < count; s++) {
        int w = sizes[s];
        pixels[s].resize(w * w * 4);
        for (int y = 0; y < w; y++) {
            for (int x = 0; x < w; x++) {
                int i = (y * w + x) * 4;
                unsigned char r = 255, g = 255, b = 255, a = 255;

                // Fries box
                int bx = (int)(5 * w / 32.0f);
                int by = (int)(16 * w / 32.0f);
                int bw = (int)(22 * w / 32.0f);
                int bh = w - by;
                if (x >= bx && x < bx + bw && y >= by) {
                    r = 220; g = 50; b = 30;
                }

                // Fries (yellow rectangles)
                struct Fry { int fx, fy, fw, fh; unsigned char cr, cg, cb; };
                Fry fries[] = {
                    {(int)(9.0f*w/32),(int)(5.0f*w/32),(int)(5.0f*w/32),(int)(15.0f*w/32),255,210,60},
                    {(int)(15.0f*w/32),(int)(3.0f*w/32),(int)(5.0f*w/32),(int)(16.0f*w/32),255,195,40},
                    {(int)(21.0f*w/32),(int)(6.0f*w/32),(int)(5.0f*w/32),(int)(13.0f*w/32),255,210,60},
                    {(int)(12.0f*w/32),(int)(8.0f*w/32),(int)(5.0f*w/32),(int)(14.0f*w/32),250,190,35},
                    {(int)(18.0f*w/32),(int)(4.0f*w/32),(int)(5.0f*w/32),(int)(15.0f*w/32),255,200,45},
                };
                for (auto& fr : fries) {
                    if (x >= fr.fx && x < fr.fx+fr.fw && y >= fr.fy && y < fr.fy+fr.fh) {
                        r = fr.cr; g = fr.cg; b = fr.cb;
                    }
                }
                pixels[s][i+0] = b;
                pixels[s][i+1] = g;
                pixels[s][i+2] = r;
                pixels[s][i+3] = a;
            }
        }
    }

    // Calculate offsets
    int headerSize = 6;
    int dirSize = count * 16;
    int offset = headerSize + dirSize;
    int imageSizes[2];
    for (int s = 0; s < count; s++) {
        imageSizes[s] = 40 + sizes[s] * sizes[s] * 4;
    }

    FILE* f = fopen("fries.ico", "wb");
    if (!f) { printf("cannot create file\n"); return 1; }

    // ICO header
    WriteLE16(f, 0);      // reserved
    WriteLE16(f, 1);      // type: ICO
    WriteLE16(f, count);  // image count

    // Directory entries
    for (int s = 0; s < count; s++) {
        int w = sizes[s];
        fputc(w == 256 ? 0 : (unsigned char)w, f);
        fputc(w == 256 ? 0 : (unsigned char)w, f);
        fputc(0, f);  // palette
        fputc(0, f);  // reserved
        WriteLE16(f, 1);   // planes
        WriteLE16(f, 32);  // bpp
        WriteLE32(f, imageSizes[s]);
        WriteLE32(f, offset);
        offset += imageSizes[s];
    }

    // Image data
    for (int s = 0; s < count; s++) {
        int w = sizes[s];
        WriteLE32(f, 40);         // biSize
        WriteLE32(f, w);          // biWidth
        WriteLE32(f, w * 2);      // biHeight (double for ICO)
        WriteLE16(f, 1);          // biPlanes
        WriteLE16(f, 32);         // biBitCount
        WriteLE32(f, 0);          // biCompression
        WriteLE32(f, w * w * 4);  // biSizeImage
        WriteLE32(f, 0);          // biXPelsPerMeter
        WriteLE32(f, 0);          // biYPelsPerMeter
        WriteLE32(f, 0);          // biClrUsed
        WriteLE32(f, 0);          // biClrImportant

        // Pixel data (bottom-up: last row first)
        for (int y = w - 1; y >= 0; y--) {
            fwrite(&pixels[s][y * w * 4], 1, w * 4, f);
        }
    }

    fclose(f);
    printf("fries.ico created (%d bytes)\n", offset);
    return 0;
}
