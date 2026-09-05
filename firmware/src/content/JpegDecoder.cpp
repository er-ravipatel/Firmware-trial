#include "JpegDecoder.h"
#include "ExifReader.h"
#include <stddef.h>

// stb entry point + our decode pool (implemented in stb_image_impl.c). All decode memory comes
// from the pool, which is reset before each decode — so nothing here is individually freed.
extern "C" {
unsigned char* stbi_load_from_memory(const unsigned char* buffer, int len,
                                     int* x, int* y, int* channels_in_file, int desired_channels);
int    stbi_info_from_memory(const unsigned char* buffer, int len, int* x, int* y, int* comp);
void   lf_pool_reset(void);
void*  lf_pool_alloc(size_t n);
size_t lf_pool_size(void);
}

namespace lf {
namespace {

const char* s_last_error = "";
unsigned    s_hdr_w = 0, s_hdr_h = 0;

// Pool bytes a decode will need: stb keeps whole-image raw planes (~1.5 B/px for 4:2:0 chroma,
// up to 3 B/px for 4:4:4) plus the 3 B/px RGB output, plus another 3 B/px if we must rotate.
// Estimate with the common 4:2:0 case; the pool's own out-of-space check is the backstop.
size_t estimate_pool_need(unsigned w, unsigned h, bool rotated) {
    size_t px = (size_t) w * h;
    return px * 3 / 2 + px * 3 + (rotated ? px * 3 : 0) + (1u << 20);
}

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
    s_last_error = "";
    s_hdr_w = s_hdr_h = 0;

    // Header-only pre-check (microseconds): reject an image the pool can't hold BEFORE spending
    // seconds decoding it only to fail at the output allocation (64 MP phone shots do exactly that).
    int w = 0, h = 0, comp = 0;
    if (!stbi_info_from_memory(data, static_cast<int>(len), &w, &h, &comp) || w <= 0 || h <= 0) {
        s_last_error = "bad header";
        return false;
    }
    s_hdr_w = unsigned(w);
    s_hdr_h = unsigned(h);
    int orient = ExifReader::orientation(data, len);
    if (estimate_pool_need(unsigned(w), unsigned(h), orient != 1) > lf_pool_size()) {
        s_last_error = "too large for decode pool";
        return false;
    }

    unsigned char* px = stbi_load_from_memory(data, static_cast<int>(len), &w, &h, &comp, 3);
    if (px == nullptr || w <= 0 || h <= 0) {
        s_last_error = "decode failed";
        return false;
    }

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

const char* JpegDecoder::last_error()    { return s_last_error; }
unsigned    JpegDecoder::last_header_w() { return s_hdr_w; }
unsigned    JpegDecoder::last_header_h() { return s_hdr_h; }

void JpegDecoder::free_image(DecodedImage& img) {
    // Pool-managed: memory is reclaimed by lf_pool_reset() before the next decode. Just detach.
    img.rgb = nullptr;
    img.w = img.h = 0;
}

}  // namespace lf
