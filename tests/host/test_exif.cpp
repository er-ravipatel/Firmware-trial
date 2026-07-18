// EXIF orientation parsing tests (crafted minimal JPEG/EXIF byte streams).
#include "test_framework.h"
#include "content/ExifReader.h"

using lf::ExifReader;

// SOI + APP1(Exif, little-endian TIFF, IFD0 with Orientation=6) + EOI.
static const uint8_t kJpegOrient6[] = {
    0xFF, 0xD8,                                     // SOI
    0xFF, 0xE1, 0x00, 0x22,                         // APP1, length 34
    'E', 'x', 'i', 'f', 0x00, 0x00,                 // "Exif\0\0"
    0x49, 0x49, 0x2A, 0x00,                         // TIFF: II (LE), magic 42
    0x08, 0x00, 0x00, 0x00,                         // IFD0 offset = 8
    0x01, 0x00,                                     // 1 entry
    0x12, 0x01, 0x03, 0x00,                         // tag 0x0112 (Orientation), type SHORT
    0x01, 0x00, 0x00, 0x00,                         // count 1
    0x06, 0x00, 0x00, 0x00,                         // value 6
    0x00, 0x00, 0x00, 0x00,                         // next IFD = 0
    0xFF, 0xD9,                                      // EOI
};

// Big-endian (MM) variant with Orientation=8.
static const uint8_t kJpegOrient8BE[] = {
    0xFF, 0xD8,
    0xFF, 0xE1, 0x00, 0x22,
    'E', 'x', 'i', 'f', 0x00, 0x00,
    0x4D, 0x4D, 0x00, 0x2A,                         // MM (BE), magic 42
    0x00, 0x00, 0x00, 0x08,                         // IFD0 offset = 8
    0x00, 0x01,                                     // 1 entry
    0x01, 0x12, 0x00, 0x03,                         // tag 0x0112, type SHORT
    0x00, 0x00, 0x00, 0x01,                         // count 1
    0x00, 0x08, 0x00, 0x00,                         // value 8 (SHORT in first 2 bytes)
    0x00, 0x00, 0x00, 0x00,
    0xFF, 0xD9,
};

// A JPEG with no EXIF (jumps straight to SOS).
static const uint8_t kJpegNoExif[] = {0xFF, 0xD8, 0xFF, 0xDA, 0x00, 0x02, 0xFF, 0xD9};

TEST("exif orientation 6 (little-endian)") {
    CHECK_EQ(ExifReader::orientation(kJpegOrient6, sizeof kJpegOrient6), 6);
}

TEST("exif orientation 8 (big-endian)") {
    CHECK_EQ(ExifReader::orientation(kJpegOrient8BE, sizeof kJpegOrient8BE), 8);
}

TEST("exif absent -> orientation 1") {
    CHECK_EQ(ExifReader::orientation(kJpegNoExif, sizeof kJpegNoExif), 1);
}

TEST("exif non-jpeg -> orientation 1") {
    const uint8_t junk[] = {0x00, 0x01, 0x02, 0x03};
    CHECK_EQ(ExifReader::orientation(junk, sizeof junk), 1);
    CHECK_EQ(ExifReader::orientation(nullptr, 0), 1);
}
