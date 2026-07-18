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

private:
    // Real Pi framebuffer is RGB (ARGB8888) — pass colors straight through. (QEMU's fb is BGR,
    // so colors look swapped in the emulator only; hardware is correct.)
    static T2DColor conv (lf::Rgb c) { return COLOR2D (c.r, c.g, c.b); }

    C2DGraphics &m_Gfx;
};

#endif
