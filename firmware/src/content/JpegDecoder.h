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

    // Why the last decode() failed ("" after a success). Static text, safe to keep a pointer to.
    static const char* last_error();
    // Dimensions from the JPEG header of the last decode() call (0 if the header was unreadable),
    // so a rejected oversized photo can still be logged with its size.
    static unsigned last_header_w();
    static unsigned last_header_h();
};

}  // namespace lf
