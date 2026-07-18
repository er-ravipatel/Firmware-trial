//
// kernel.cpp — Lumen Frame boot splash.
//
#include "kernel.h"
#include <circle/timer.h>
#include <circle/string.h>

#define LUMEN_VERSION "v0.0.1"

CKernel::CKernel (void)
:   m_Screen (m_Options.GetWidth (), m_Options.GetHeight ())
{
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
    return m_Screen.Initialize ();
}

void CKernel::Print (const char *pString)
{
    size_t nLen = 0;
    while (pString[nLen] != '\0') nLen++;
    m_Screen.Write (pString, nLen);
}

void CKernel::FillRect (unsigned x, unsigned y, unsigned w, unsigned h, TScreenColor color)
{
    for (unsigned dy = 0; dy < h; dy++)
    {
        for (unsigned dx = 0; dx < w; dx++)
        {
            m_Screen.SetPixel (x + dx, y + dy, color);
        }
    }
}

void CKernel::DrawBorder (TScreenColor color)
{
    const unsigned W = m_Screen.GetWidth ();
    const unsigned H = m_Screen.GetHeight ();
    for (unsigned x = 0; x < W; x++)
    {
        m_Screen.SetPixel (x, 0, color);
        m_Screen.SetPixel (x, H - 1, color);
    }
    for (unsigned y = 0; y < H; y++)
    {
        m_Screen.SetPixel (0, y, color);
        m_Screen.SetPixel (W - 1, y, color);
    }
}

void CKernel::DrawSplash (void)
{
    const unsigned W = m_Screen.GetWidth ();
    const unsigned H = m_Screen.GetHeight ();

    DrawBorder (NORMAL_COLOR);

    // Accent bar under the title area.
    FillRect (0, 64, W, 6, CYAN_COLOR);

    // Color swatches near the bottom — proves color rendering across the framebuffer.
    static const TScreenColor Swatches[] =
        { RED_COLOR, GREEN_COLOR, BLUE_COLOR, YELLOW_COLOR, MAGENTA_COLOR, CYAN_COLOR };
    const unsigned sw = 60, sh = 40, sy = H - 80, sx = 40, gap = 12;
    for (unsigned i = 0; i < 6; i++)
    {
        FillRect (sx + i * (sw + gap), sy, sw, sh, Swatches[i]);
    }

    // Title + status text (console). Leading newlines push it below the accent bar.
    Print ("\n\n\n\n\n");
    Print ("        LUMEN FRAME\n\n");
    Print ("        Bare-metal firmware OS\n");
    Print ("        Circle + QEMU   |   Pi Zero 2 W (BCM2837)\n\n");

    CString Status;
    Status.Format ("        [ boot OK ]  framebuffer %u x %u\n", W, H);
    m_Screen.Write ((const char *) Status, Status.GetLength ());

    Print ("        " LUMEN_VERSION "\n");
}

TShutdownMode CKernel::Run (void)
{
    DrawSplash ();

    // Heartbeat: blink the activity LED so a live device shows it's running.
    while (1)
    {
        m_ActLED.On ();
        CTimer::SimpleMsDelay (200);
        m_ActLED.Off ();
        CTimer::SimpleMsDelay (200);
    }

    return ShutdownHalt;
}
