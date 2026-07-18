#include "ExifReader.h"

namespace lf {

namespace {

// Read 16-/32-bit integers with a given endianness.
inline uint16_t rd16(const uint8_t* p, bool le) {
    return le ? uint16_t(p[0] | (p[1] << 8)) : uint16_t((p[0] << 8) | p[1]);
}
inline uint32_t rd32(const uint8_t* p, bool le) {
    return le ? (uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24))
              : (uint32_t(p[3]) | (uint32_t(p[2]) << 8) | (uint32_t(p[1]) << 16) | (uint32_t(p[0]) << 24));
}

}  // namespace

int ExifReader::orientation(const uint8_t* data, unsigned len) {
    if (data == nullptr || len < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return 1;  // not a JPEG
    }

    unsigned pos = 2;
    while (pos + 4 <= len) {
        if (data[pos] != 0xFF) {
            return 1;  // marker desync
        }
        uint8_t marker = data[pos + 1];
        if (marker == 0xD9 || marker == 0xDA) {
            break;  // EOI or start of scan — no EXIF before this
        }
        unsigned seglen = (data[pos + 2] << 8) | data[pos + 3];
        if (seglen < 2 || pos + 2 + seglen > len) {
            return 1;
        }
        const uint8_t* seg = data + pos + 4;
        unsigned segdata = seglen - 2;

        // APP1 with "Exif\0\0" header.
        if (marker == 0xE1 && segdata >= 6 &&
            seg[0] == 'E' && seg[1] == 'x' && seg[2] == 'i' && seg[3] == 'f' &&
            seg[4] == 0 && seg[5] == 0) {
            const uint8_t* tiff = seg + 6;
            unsigned tifflen = segdata - 6;
            if (tifflen < 8) return 1;

            bool le;
            if (tiff[0] == 'I' && tiff[1] == 'I') le = true;
            else if (tiff[0] == 'M' && tiff[1] == 'M') le = false;
            else return 1;

            uint32_t ifd0 = rd32(tiff + 4, le);
            if (ifd0 + 2 > tifflen) return 1;

            uint16_t nEntries = rd16(tiff + ifd0, le);
            unsigned entry = ifd0 + 2;
            for (unsigned i = 0; i < nEntries; ++i, entry += 12) {
                if (entry + 12 > tifflen) break;
                uint16_t tag = rd16(tiff + entry, le);
                if (tag == 0x0112) {  // Orientation
                    uint16_t value = rd16(tiff + entry + 8, le);
                    if (value >= 1 && value <= 8) return int(value);
                    return 1;
                }
            }
            return 1;  // Exif present but no orientation
        }

        pos += 2 + seglen;
    }
    return 1;
}

}  // namespace lf
