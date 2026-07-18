#include "PhotoFramePlugin.h"
#include "../display/ICanvas.h"
#include "../content/test_image.h"   // embedded baseline JPEG (lumen_test_jpg[])

namespace lf {

void PhotoFramePlugin::ensure_decoded() {
    if (tried_) {
        return;
    }
    tried_ = true;
    JpegDecoder::decode(lumen_test_jpg, lumen_test_jpg_len, img_);
}

void PhotoFramePlugin::render(ICanvas& canvas) {
    ensure_decoded();

    const unsigned W = canvas.width();
    const unsigned H = canvas.height();

    canvas.clear(rgb::DarkGray);

    // Framed "photo" area centered in the screen.
    const unsigned fw = W * 3 / 4;
    const unsigned fh = H * 3 / 5;
    const unsigned fx = (W - fw) / 2;
    const unsigned fy = (H - fh) / 2 - 8;

    canvas.fill_rect(fx - 6, fy - 6, fw + 12, fh + 12, rgb::White);

    if (img_.rgb != nullptr && img_.w > 0 && img_.h > 0) {
        // Centre the decoded image within the frame (image fits by construction).
        unsigned iw = img_.w < fw ? img_.w : fw;
        unsigned ih = img_.h < fh ? img_.h : fh;
        unsigned ix = fx + (fw - iw) / 2;
        unsigned iy = fy + (fh - ih) / 2;
        canvas.blit_rgb(ix, iy, iw, ih, img_.rgb);
    } else {
        // Fallback: placeholder color bands if decode failed.
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
    const char* prefix = "Photo ";
    unsigned k = 0;
    for (const char* p = prefix; *p; ++p) caption[k++] = *p;
    caption[k++] = char('1' + index_);
    caption[k++] = '/';
    caption[k++] = '3';
    const char* suffix = (img_.rgb ? "   JPEG decoded (stb_image)   " : "   decode failed   ");
    for (const char* p = suffix; *p; ++p) caption[k++] = *p;
    caption[k] = '\0';
    canvas.text(40, H - 28, caption, rgb::White);
}

}  // namespace lf
