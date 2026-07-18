// CircleCanvas — ICanvas backed by Circle's C2DGraphics (double-buffered framebuffer with
// pixel-accurate text). Plugins draw here; the kernel calls present() to flip the buffer.
#ifndef _circlecanvas_h
#define _circlecanvas_h

#include <circle/2dgraphics.h>
#include "display/ICanvas.h"   // via EXTRAINCLUDE=-I../src

class CircleCanvas : public lf::ICanvas
{
public:
    explicit CircleCanvas (C2DGraphics &Gfx) : m_Gfx (Gfx) {}

    unsigned width () const override  { return m_Gfx.GetWidth (); }
    unsigned height () const override { return m_Gfx.GetHeight (); }

    void clear (lf::Rgb c) override
    {
        m_Gfx.ClearScreen (conv (c));
    }
    void set_pixel (unsigned x, unsigned y, lf::Rgb c) override
    {
        m_Gfx.DrawPixel (x, y, conv (c));
    }
    void fill_rect (unsigned x, unsigned y, unsigned w, unsigned h, lf::Rgb c) override
    {
        m_Gfx.DrawRect (x, y, w, h, conv (c));
    }
    void text (unsigned x, unsigned y, const char *s, lf::Rgb c) override
    {
        m_Gfx.DrawText (x, y, conv (c), s);
    }
    void present () override
    {
        m_Gfx.UpdateDisplay ();
    }

    // Fast full-rectangle blits: write straight into the 32-bit back buffer (no per-pixel
    // DrawPixel call). This is the hot path for the slideshow.
    void blit_rgb (unsigned x, unsigned y, unsigned w, unsigned h, const u8 *rgb) override
    {
        u32 *buf = (u32 *) m_Gfx.GetBuffer ();
        unsigned fbw = m_Gfx.GetWidth (), fbh = m_Gfx.GetHeight ();
        for (unsigned r = 0; r < h; ++r)
        {
            unsigned py = y + r;
            if (py >= fbh) break;
            u32 *d = buf + (unsigned long) py * fbw + x;
            const u8 *s = rgb + (unsigned long) r * w * 3;
            for (unsigned c = 0; c < w; ++c, s += 3)
            {
                d[c] = 0xFF000000u | COLOR2D (s[0], s[1], s[2]);
            }
        }
    }

    void blit_rgb_blend (unsigned x, unsigned y, unsigned w, unsigned h,
                         const u8 *a, const u8 *b, unsigned alpha) override
    {
        unsigned ia = 256 - alpha;
        u32 *buf = (u32 *) m_Gfx.GetBuffer ();
        unsigned fbw = m_Gfx.GetWidth (), fbh = m_Gfx.GetHeight ();
        for (unsigned r = 0; r < h; ++r)
        {
            unsigned py = y + r;
            if (py >= fbh) break;
            u32 *d = buf + (unsigned long) py * fbw + x;
            const u8 *pa = a + (unsigned long) r * w * 3;
            const u8 *pb = b + (unsigned long) r * w * 3;
            for (unsigned c = 0; c < w; ++c, pa += 3, pb += 3)
            {
                unsigned rr = (pa[0] * ia + pb[0] * alpha) >> 8;
                unsigned gg = (pa[1] * ia + pb[1] * alpha) >> 8;
                unsigned bb = (pa[2] * ia + pb[2] * alpha) >> 8;
                d[c] = 0xFF000000u | COLOR2D (rr, gg, bb);
            }
        }
    }

private:
    // Real Pi framebuffer is RGB (ARGB8888) — pass colors straight through. (QEMU's fb is BGR,
    // so colors look swapped in the emulator only; hardware is correct.)
    static T2DColor conv (lf::Rgb c) { return COLOR2D (c.r, c.g, c.b); }

    C2DGraphics &m_Gfx;
};

#endif
