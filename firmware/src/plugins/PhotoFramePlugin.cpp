#include "PhotoFramePlugin.h"
#include "../display/ICanvas.h"

namespace lf {

void PhotoFramePlugin::render(ICanvas& canvas) {
    const unsigned W = canvas.width();
    const unsigned H = canvas.height();

    canvas.clear(rgb::DarkGray);

    // A framed "photo" centered in the screen.
    const unsigned fw = W * 3 / 4;
    const unsigned fh = H * 3 / 5;
    const unsigned fx = (W - fw) / 2;
    const unsigned fy = (H - fh) / 2 - 8;

    // White mat/frame.
    canvas.fill_rect(fx - 6, fy - 6, fw + 12, fh + 12, rgb::White);

    // "Photo" content: three theme palettes, cycled per activation, as vertical bands.
    static const Rgb themes[kCount][3] = {
        {rgb::Blue, rgb::Cyan, rgb::Green},
        {rgb::Magenta, rgb::Red, rgb::Yellow},
        {rgb::Green, rgb::Yellow, rgb::Cyan},
    };
    const Rgb* pal = themes[index_];
    const unsigned band = fw / 3;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned bw = (i == 2) ? (fw - band * 2) : band;
        canvas.fill_rect(fx + i * band, fy, bw, fh, pal[i]);
    }

    // Labels (pixel coordinates).
    canvas.text(40, 12, "PHOTOS", rgb::White);
    char caption[64];
    // "Photo X/3   photo plugin   (cycles every 2s)"
    const char* prefix = "Photo ";
    unsigned k = 0;
    for (const char* p = prefix; *p; ++p) caption[k++] = *p;
    caption[k++] = char('1' + index_);
    caption[k++] = '/';
    caption[k++] = '3';
    const char* suffix = "   photo plugin   (cycles every 2s)";
    for (const char* p = suffix; *p; ++p) caption[k++] = *p;
    caption[k] = '\0';
    canvas.text(40, H - 28, caption, rgb::White);
}

}  // namespace lf
