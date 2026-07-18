#include "PhotoFramePlugin.h"
#include "../display/ICanvas.h"
#include "../content/test_image.h"   // embedded fallback JPEG (lumen_test_jpg[])

namespace lf {

void PhotoFramePlugin::advance() {
    unsigned n = photo_count();
    if (n > 0) {
        index_ = (index_ + 1) % int(n);
    } else {
        index_ = 0;
    }
    decoded_ = false;
}

void PhotoFramePlugin::decode_current() {
    if (decoded_) {
        return;
    }
    decoded_ = true;
    JpegDecoder::free_image(img_);

    const uint8_t* data = nullptr;
    unsigned len = 0;
    if (photo_count() > 0) {
        data = source_->jpeg(unsigned(index_), len);
    }
    if (data == nullptr) {              // no source, or load failed -> embedded fallback
        data = lumen_test_jpg;
        len = lumen_test_jpg_len;
    }
    JpegDecoder::decode(data, len, img_);
}

void PhotoFramePlugin::render(ICanvas& canvas) {
    decode_current();

    const unsigned W = canvas.width();
    const unsigned H = canvas.height();

    canvas.clear(rgb::DarkGray);

    // Framed area centered in the screen.
    const unsigned fw = W * 3 / 4;
    const unsigned fh = H * 3 / 5;
    const unsigned fx = (W - fw) / 2;
    const unsigned fy = (H - fh) / 2 - 8;
    canvas.fill_rect(fx - 6, fy - 6, fw + 12, fh + 12, rgb::White);

    if (img_.rgb != nullptr && img_.w > 0 && img_.h > 0) {
        // Fit-to-frame preserving aspect ratio (integer math).
        unsigned dw = fw, dh = img_.h * fw / img_.w;
        if (dh > fh) {
            dh = fh;
            dw = img_.w * fh / img_.h;
        }
        unsigned ix = fx + (fw - dw) / 2;
        unsigned iy = fy + (fh - dh) / 2;
        // Black bars behind, so letterboxing looks intentional.
        canvas.fill_rect(fx, fy, fw, fh, rgb::Black);
        canvas.blit_rgb_scaled(ix, iy, dw, dh, img_.rgb, img_.w, img_.h);
    } else {
        static const Rgb pal[3] = {rgb::Blue, rgb::Cyan, rgb::Green};
        const unsigned band = fw / 3;
        for (unsigned i = 0; i < 3; ++i) {
            unsigned bw = (i == 2) ? (fw - band * 2) : band;
            canvas.fill_rect(fx + i * band, fy, bw, fh, pal[i]);
        }
    }

    // Labels.
    canvas.text(40, 12, "PHOTOS", rgb::White);
    char caption[80];
    unsigned k = 0;
    unsigned n = photo_count();
    if (n > 0) {
        const char* pfx = "Photo ";
        for (const char* p = pfx; *p; ++p) caption[k++] = *p;
        caption[k++] = char('0' + (index_ + 1) / 10 % 10);
        caption[k++] = char('0' + (index_ + 1) % 10);
        caption[k++] = '/';
        caption[k++] = char('0' + (n / 10) % 10);
        caption[k++] = char('0' + n % 10);
        const char* sfx = "   slideshow from SD card";
        for (const char* p = sfx; *p; ++p) caption[k++] = *p;
    } else {
        const char* s = "Photo (embedded fallback)";
        for (const char* p = s; *p; ++p) caption[k++] = *p;
    }
    caption[k] = '\0';
    canvas.text(40, H - 28, caption, rgb::White);
}

}  // namespace lf
