#include "JpegDecoder.h"

// stb_image entry points (implemented in stb_image_impl.c). Declared here to avoid
// pulling the large stb header into C++ translation units.
extern "C" {
unsigned char* stbi_load_from_memory(const unsigned char* buffer, int len,
                                     int* x, int* y, int* channels_in_file,
                                     int desired_channels);
void stbi_image_free(void* retval_from_stbi_load);
}

namespace lf {

bool JpegDecoder::decode(const uint8_t* data, unsigned len, DecodedImage& out) {
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory(data, static_cast<int>(len), &w, &h, &comp, 3);
    if (px == nullptr || w <= 0 || h <= 0) {
        return false;
    }
    out.w = static_cast<unsigned>(w);
    out.h = static_cast<unsigned>(h);
    out.rgb = px;
    return true;
}

void JpegDecoder::free_image(DecodedImage& img) {
    if (img.rgb != nullptr) {
        stbi_image_free(img.rgb);
        img.rgb = nullptr;
    }
}

}  // namespace lf
