#include "JpegDecoder.h"
#include "ExifReader.h"
#include <stdlib.h>

// stb_image entry point (implemented in stb_image_impl.c). stb uses the system malloc/free,
// so free() below is correct for buffers it returns as well as our own.
extern "C" {
unsigned char* stbi_load_from_memory(const unsigned char* buffer, int len,
                                     int* x, int* y, int* channels_in_file,
                                     int desired_channels);
}

namespace lf {
namespace {

// Produce a new RGB buffer with the EXIF orientation applied (upright). Returns nullptr on
// allocation failure; sets ow/oh to the output dimensions (swapped for 90/270 rotations).
uint8_t* apply_orientation(const uint8_t* src, unsigned w, unsigned h, int orient,
                           unsigned& ow, unsigned& oh) {
    bool swap = (orient >= 5 && orient <= 8);
    ow = swap ? h : w;
    oh = swap ? w : h;
    uint8_t* dst = (uint8_t*) malloc((unsigned long) ow * oh * 3);
    if (dst == nullptr) {
        return nullptr;
    }
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            unsigned dx, dy;
            switch (orient) {
                default:
                case 1: dx = x;         dy = y;         break;  // normal
                case 2: dx = w - 1 - x; dy = y;         break;  // mirror H
                case 3: dx = w - 1 - x; dy = h - 1 - y; break;  // 180
                case 4: dx = x;         dy = h - 1 - y; break;  // mirror V
                case 5: dx = y;         dy = x;         break;  // transpose
                case 6: dx = h - 1 - y; dy = x;         break;  // 90 CW
                case 7: dx = h - 1 - y; dy = w - 1 - x; break;  // transverse
                case 8: dx = y;         dy = w - 1 - x; break;  // 90 CCW
            }
            const uint8_t* s = src + ((unsigned long) y * w + x) * 3;
            uint8_t* d = dst + ((unsigned long) dy * ow + dx) * 3;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
    }
    return dst;
}

}  // namespace

bool JpegDecoder::decode(const uint8_t* data, unsigned len, DecodedImage& out) {
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory(data, static_cast<int>(len), &w, &h, &comp, 3);
    if (px == nullptr || w <= 0 || h <= 0) {
        return false;
    }

    int orient = ExifReader::orientation(data, len);
    if (orient != 1) {
        unsigned ow = 0, oh = 0;
        uint8_t* rotated = apply_orientation(px, unsigned(w), unsigned(h), orient, ow, oh);
        if (rotated != nullptr) {
            free(px);
            out.w = ow;
            out.h = oh;
            out.rgb = rotated;
            return true;
        }
        // rotation allocation failed -> fall through with the unrotated image
    }

    out.w = unsigned(w);
    out.h = unsigned(h);
    out.rgb = px;
    return true;
}

void JpegDecoder::free_image(DecodedImage& img) {
    if (img.rgb != nullptr) {
        free(img.rgb);
        img.rgb = nullptr;
    }
}

}  // namespace lf
