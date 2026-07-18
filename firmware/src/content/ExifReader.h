// ExifReader — extracts the EXIF Orientation tag (1..8) from a JPEG in memory.
// Used to display photos upright (phone photos are usually stored rotated). Returns 1
// (normal) if no EXIF/orientation is found.
#pragma once
#include <stdint.h>

namespace lf {

class ExifReader {
public:
    // Returns the EXIF orientation value 1..8, or 1 if absent/unparseable.
    static int orientation(const uint8_t* data, unsigned len);
};

}  // namespace lf
