// ICanvas — device-neutral drawing surface that ScreenPlugins draw onto.
// The firmware backs this with a Circle framebuffer (CircleCanvas); host tests could back
// it with an in-memory buffer. Colors are plain RGB so plugins stay hardware-agnostic.
#pragma once
#include <stdint.h>

namespace lf {

struct Rgb {
    uint8_t r, g, b;
};

namespace rgb {
inline constexpr Rgb Black   {0, 0, 0};
inline constexpr Rgb White   {255, 255, 255};
inline constexpr Rgb Red     {210, 50, 50};
inline constexpr Rgb Green   {60, 190, 90};
inline constexpr Rgb Blue    {70, 120, 230};
inline constexpr Rgb Yellow  {235, 200, 70};
inline constexpr Rgb Cyan    {70, 200, 210};
inline constexpr Rgb Magenta {200, 80, 190};
inline constexpr Rgb DarkGray{22, 24, 30};
inline constexpr Rgb Navy    {12, 18, 46};
}  // namespace rgb

class ICanvas {
public:
    virtual ~ICanvas() = default;

    virtual unsigned width() const = 0;
    virtual unsigned height() const = 0;

    virtual void clear(Rgb c) = 0;
    virtual void set_pixel(unsigned x, unsigned y, Rgb c) = 0;
    virtual void fill_rect(unsigned x, unsigned y, unsigned w, unsigned h, Rgb c) = 0;

    // Text with the top-left at pixel (x, y), default font, transparent background.
    virtual void text(unsigned x, unsigned y, const char* s, Rgb c) = 0;

    // Blit an RGB888 buffer (w*h*3, top-down) with its top-left at (x, y). Default
    // implementation is per-pixel; backends may override for speed.
    virtual void blit_rgb(unsigned x, unsigned y, unsigned w, unsigned h, const uint8_t* rgb) {
        for (unsigned row = 0; row < h; ++row) {
            for (unsigned col = 0; col < w; ++col) {
                const uint8_t* p = rgb + (static_cast<unsigned long>(row) * w + col) * 3;
                set_pixel(x + col, y + row, Rgb{p[0], p[1], p[2]});
            }
        }
    }

    // Flush the back buffer to the display (double-buffered backends). Default: no-op.
    virtual void present() {}
};

}  // namespace lf
