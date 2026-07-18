#include "JpegDecoder.h"
#include "ExifReader.h"
#include <stddef.h>

// stb entry point + our decode pool (implemented in stb_image_impl.c). All decode memory comes
// from the pool, which is reset before each decode — so nothing here is individually freed.
extern "C" {
unsigned char* stbi_load_from_memory(const unsigned char* buffer, int len,
                                     int* x, int* y, int* channels_in_file, int desired_channels);
void  lf_pool_reset(void);
void* lf_pool_alloc(size_t n);
}

namespace lf {
namespace {

// Apply EXIF orientation into a fresh pool buffer.
uint8_t* apply_orientation(const uint8_t* src, unsigned w, unsigned h, int orient,
                           unsigned& ow, unsigned& oh) {
    bool swap = (orient >= 5 && orient <= 8);
    ow = swap ? h : w;
    oh = swap ? w : h;
    uint8_t* dst = (uint8_t*) lf_pool_alloc((size_t) ow * oh * 3);
    if (dst == nullptr) return nullptr;
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            unsigned dx, dy;
            switch (orient) {
                default:
                case 1: dx = x;         dy = y;         break;
                case 2: dx = w - 1 - x; dy = y;         break;
                case 3: dx = w - 1 - x; dy = h - 1 - y; break;
                case 4: dx = x;         dy = h - 1 - y; break;
                case 5: dx = y;         dy = x;         break;
                case 6: dx = h - 1 - y; dy = x;         break;
                case 7: dx = h - 1 - y; dy = w - 1 - x; break;
                case 8: dx = y;         dy = w - 1 - x; break;
            }
            const uint8_t* s = src + ((unsigned long) y * w + x) * 3;
            uint8_t* d = dst + ((unsigned long) dy * ow + dx) * 3;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        }
    }
    return dst;
}

}  // namespace

bool JpegDecoder::decode(const uint8_t* data, unsigned len, DecodedImage& out) {
    lf_pool_reset();   // reclaim the previous decode's pool memory (already consumed by caller)

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
            out.w = ow;
            out.h = oh;
            out.rgb = rotated;
            return true;
        }
    }
    out.w = unsigned(w);
    out.h = unsigned(h);
    out.rgb = px;
    return true;
}

void JpegDecoder::free_image(DecodedImage& img) {
    // Pool-managed: memory is reclaimed by lf_pool_reset() before the next decode. Just detach.
    img.rgb = nullptr;
    img.w = img.h = 0;
}

}  // namespace lf
