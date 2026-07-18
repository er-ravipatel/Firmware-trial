#include "ClockPlugin.h"
#include "../display/ICanvas.h"

namespace lf {

static void two_digits(char* out, unsigned v) {
    out[0] = char('0' + (v / 10) % 10);
    out[1] = char('0' + v % 10);
}

void ClockPlugin::render(ICanvas& canvas) {
    const unsigned W = canvas.width();
    const unsigned H = canvas.height();

    canvas.clear(rgb::Navy);

    // A centered panel.
    const unsigned pw = W * 3 / 5;
    const unsigned ph = H / 3;
    const unsigned px = (W - pw) / 2;
    const unsigned py = (H - ph) / 2;
    canvas.fill_rect(px, py, pw, ph, rgb::DarkGray);
    // Accent underline.
    canvas.fill_rect(px, py + ph, pw, 5, rgb::Cyan);

    const uint32_t ms = ms_ ? *ms_ : 0;
    const unsigned secs = ms / 1000;
    const unsigned mm = (secs / 60) % 100;
    const unsigned ss = secs % 60;

    char clock[6];
    two_digits(&clock[0], mm);
    clock[2] = ':';
    two_digits(&clock[3], ss);
    clock[5] = '\0';

    canvas.text(px + 16, py + 14, "CLOCK", rgb::White);
    // Roughly centre the MM:SS text (default font ~8px wide, 5 glyphs).
    canvas.text(W / 2 - 20, H / 2 - 6, clock, rgb::White);
    canvas.text(40, H - 28, "clock plugin   time since boot   (cycles every 2s)", rgb::White);
}

}  // namespace lf
