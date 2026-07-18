//
// kernel.cpp — Lumen Frame modular display loop (with SD/FatFs photo loading).
//
#include "kernel.h"
#include <circle/timer.h>
#include <circle/string.h>

static const char FromKernel[] = "kernel";

// Fake time-of-day for scheduling until an RTC/NTP source exists (10:00).
static const unsigned kNowMin = 600;
static const unsigned kTickMs = 100;

CKernel::CKernel (void)
:   m_Timer (&m_Interrupt),
    m_Logger (m_Options.GetLogLevel (), &m_Timer),
    m_USBHCI (&m_Interrupt, &m_Timer),
    m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
    m_Graphics (m_Options.GetWidth (), m_Options.GetHeight (), FALSE),
    m_Canvas (m_Graphics),
    m_ElapsedMs (0),
    m_Clock (&m_ElapsedMs),
    m_PluginCount (0)
{
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
    boolean bOK = TRUE;

    // Essential subsystems — a failure here aborts boot.
    if (bOK) bOK = m_Serial.Initialize (115200);
    if (bOK) bOK = m_Logger.Initialize (&m_Serial);
    if (bOK) bOK = m_Interrupt.Initialize ();
    if (bOK) bOK = m_Timer.Initialize ();
    if (bOK) bOK = m_Graphics.Initialize ();

    // Storage is OPTIONAL: the frame must still boot (embedded image) with no USB and no SD.
    m_USBHCI.Initialize ();   // enumerates a USB pendrive if present
    m_EMMC.Initialize ();     // SD card if present (returns false when absent)

    return bOK;
}

void CKernel::SetupPlugins (void)
{
    m_Photo.set_clock (&m_ElapsedMs);          // drives dwell + cross-fade timing

    m_Plugins[0] = &m_Photo;
    m_Scheduler.add ({"photo", true, 24, -1, -1});   // slideshow is the star

    m_Plugins[1] = &m_Clock;
    m_Scheduler.add ({"clock", true, 4, -1, -1});     // brief clock interlude

    m_PluginCount = 2;
}

void CKernel::Activate (int nIndex)
{
    if (nIndex < 0)
    {
        return;
    }
    m_Plugins[nIndex]->on_activate ();
    m_Plugins[nIndex]->render (m_Canvas);
    m_Canvas.present ();
}

void CKernel::BootMessage (unsigned &nY, const char *pMsg, lf::Rgb Color)
{
    m_Canvas.text (60, nY, pMsg, Color);
    m_Canvas.present ();
    nY += 26;
    CTimer::SimpleMsDelay (650);   // deliberately paced so the boot is watchable
}

TShutdownMode CKernel::Run (void)
{
    const unsigned W = m_Canvas.width ();

    // ---- Visible boot sequence (bare metal boots in well under a second; this is paced
    // on purpose so the steps are watchable on screen) ----
    m_Canvas.clear (lf::rgb::Navy);
    m_Canvas.text (60, 54, "LUMEN FRAME", lf::rgb::Cyan);
    m_Canvas.text (60, 82, "bare-metal firmware OS   (Circle / Pi Zero 2 W)", lf::rgb::White);
    m_Canvas.fill_rect (60, 110, W - 120, 3, lf::rgb::Cyan);
    m_Canvas.present ();
    CTimer::SimpleMsDelay (1000);

    unsigned nY = 144;
    BootMessage (nY, "[ok]  CPU cores + MMU + framebuffer", lf::rgb::Green);

    // Prefer a USB pendrive; fall back to the SD card; then the embedded image.
    unsigned nPhotos = 0;
    const char *pSource = "embedded fallback";

    boolean bUSB = (f_mount (&m_FileSystemUSB, "USB:", 1) == FR_OK);
    if (bUSB)
    {
        m_PhotoSource.Scan ("USB:/");
    }
    unsigned nUSB = bUSB ? m_PhotoSource.count () : 0;

    if (nUSB > 0)
    {
        nPhotos = nUSB;
        pSource = "USB pendrive";
        m_Photo.set_source (&m_PhotoSource);
        BootMessage (nY, "[ok]  USB pendrive detected", lf::rgb::Green);
    }
    else
    {
        BootMessage (nY, bUSB ? "[--]  USB drive present, no photos"
                              : "[--]  USB pendrive: not detected", lf::rgb::Yellow);

        boolean bSD = (f_mount (&m_FileSystemSD, "SD:", 1) == FR_OK);
        if (bSD)
        {
            m_PhotoSource.Scan ("SD:/");
            nPhotos = m_PhotoSource.count ();
            if (nPhotos > 0)
            {
                pSource = "SD card";
                m_Photo.set_source (&m_PhotoSource);
            }
        }
        BootMessage (nY, bSD ? "[ok]  SD card mounted (FatFs)"
                             : "[!!]  SD card mount FAILED", bSD ? lf::rgb::Green : lf::rgb::Red);
    }

    CString PhotoMsg;
    PhotoMsg.Format ("[ok]  photos: %u   (source: %s)", nPhotos, pSource);
    BootMessage (nY, (const char *) PhotoMsg, nPhotos ? lf::rgb::Green : lf::rgb::Yellow);

    m_Logger.Write (FromKernel, LogNotice, "Boot: source=%s photos=%u", pSource, nPhotos);

    BootMessage (nY, "[ok]  plugins registered: photo, clock", lf::rgb::Green);
    BootMessage (nY, ">>>   starting slideshow ...", lf::rgb::White);
    CTimer::SimpleMsDelay (1000);

    SetupPlugins ();

    Activate (m_Scheduler.next (kNowMin));
    unsigned nSinceSwitchMs = 0;

    while (1)
    {
        CTimer::SimpleMsDelay (kTickMs);
        m_ElapsedMs += kTickMs;
        nSinceSwitchMs += kTickMs;

        int nCur = m_Scheduler.current ();
        if (nCur >= 0 && m_Plugins[nCur]->wants_continuous_redraw ())
        {
            m_Plugins[nCur]->update ();
            m_Plugins[nCur]->render (m_Canvas);
            m_Canvas.present ();
        }

        unsigned nDurMs = (nCur >= 0) ? m_Scheduler.at (nCur).duration_s * 1000u : 1000u;
        if (nSinceSwitchMs >= nDurMs)
        {
            if (nCur >= 0)
            {
                m_Plugins[nCur]->on_deactivate ();
            }
            Activate (m_Scheduler.next (kNowMin));
            nSinceSwitchMs = 0;
        }
    }

    return ShutdownHalt;
}
