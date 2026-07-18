// JpegDecoder — decodes a JPEG (in memory) to an RGB888 buffer. Backed by stb_image.
// The RGB buffer is heap-allocated; call free_image() when done.
#pragma once
#include <stdint.h>

namespace lf {

struct DecodedImage {
    unsigned w = 0;
    unsigned h = 0;
    uint8_t* rgb = nullptr;   // w*h*3 bytes, RGB888, top-down
};

class JpegDecoder {
public:
    // Returns true and fills `out` on success. On failure returns false, out unchanged.
    static bool decode(const uint8_t* data, unsigned len, DecodedImage& out);
    static void free_image(DecodedImage& img);
};

}  // namespace lf
