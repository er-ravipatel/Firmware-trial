//
// kernel.cpp — Lumen Frame modular display loop (with SD/FatFs photo loading).
//
#include "kernel.h"
#include <circle/timer.h>
#include <circle/string.h>

static const char FromKernel[] = "kernel";

// Fake time-of-day for scheduling until an RTC/NTP source exists (10:00).
static const unsigned kNowMin = 600;

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
    m_Photo.set_time_us (&CTimer::GetClockTicks);   // microsecond clock for perf logging

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

// Scan a drive ("SD:" / "USB:") for photos, preferring a photos/ then images/ subfolder,
// falling back to the drive root. Returns the number of photos found.
unsigned CKernel::ScanPhotos (const char *pDrive)
{
    static const char *s_Subs[] = {"/photos", "/images", "/"};
    for (unsigned i = 0; i < 3; i++)
    {
        CString Dir;
        Dir.Format ("%s%s", pDrive, s_Subs[i]);
        m_PhotoSource.Scan ((const char *) Dir);
        if (m_PhotoSource.count () > 0)
        {
            return m_PhotoSource.count ();
        }
    }
    return 0;
}

void CKernel::BootMessage (unsigned &nY, const char *pMsg, lf::Rgb Color)
{
    m_Canvas.text (60, nY, pMsg, Color);
    m_Canvas.present ();
    m_Logger.Write (FromKernel, LogNotice, "%s", pMsg);   // also to the SD log file
    nY += 26;
    CTimer::SimpleMsDelay (350);   // paced so the boot is watchable (log captures everything)
}

TShutdownMode CKernel::Run (void)
{
    const unsigned W = m_Canvas.width ();

    // ---- Set up SD-card file logging FIRST, and retarget the logger to it, so everything
    // below (incl. any Circle panic/exception) is captured in SD:/lumenlog.txt with f_sync. ----
    boolean bSDMounted = (f_mount (&m_FileSystemSD, "SD:", 1) == FR_OK);
    boolean bLogOpen = bSDMounted && m_FileLog.Open ("SD:/lumenlog.txt");
    if (bLogOpen)
    {
        m_Logger.SetNewTarget (&m_FileLog);
    }
    m_Logger.Write (FromKernel, LogNotice,
                    "==== Lumen Frame boot ==== SDmount=%d log=%d", (int) bSDMounted, (int) bLogOpen);

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
    unsigned nUSB = bUSB ? ScanPhotos ("USB:") : 0;

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

        if (bSDMounted)   // already mounted at the top for logging
        {
            nPhotos = ScanPhotos ("SD:");
            if (nPhotos > 0)
            {
                pSource = "SD card";
                m_Photo.set_source (&m_PhotoSource);
            }
        }
        BootMessage (nY, bSDMounted ? "[ok]  SD card mounted (FatFs)"
                                    : "[!!]  SD card mount FAILED", bSDMounted ? lf::rgb::Green : lf::rgb::Red);
    }

    CString PhotoMsg;
    PhotoMsg.Format ("[ok]  photos: %u   (source: %s)", nPhotos, pSource);
    BootMessage (nY, (const char *) PhotoMsg, nPhotos ? lf::rgb::Green : lf::rgb::Yellow);

    m_Logger.Write (FromKernel, LogNotice, "Boot: source=%s photos=%u", pSource, nPhotos);

    BootMessage (nY, "[ok]  plugins registered: photo, clock", lf::rgb::Green);
    BootMessage (nY, ">>>   starting slideshow ...", lf::rgb::White);
    CTimer::SimpleMsDelay (600);

    SetupPlugins ();

    unsigned nSwitchAtMs = CTimer::GetClockTicks () / 1000;
    m_ElapsedMs = nSwitchAtMs;
    Activate (m_Scheduler.next (kNowMin));

    // Per-second frame-timing accumulators (all microseconds).
    unsigned nStatStartMs = m_ElapsedMs;
    unsigned nFrames = 0, nFadeFrames = 0;
    unsigned nRenderSum = 0, nRenderMax = 0, nPresentSum = 0, nPresentMax = 0;

    while (1)
    {
        // Real elapsed time drives smooth Ken Burns + cross-fade animation (frame-rate
        // independent), instead of a fixed tick.
        m_ElapsedMs = CTimer::GetClockTicks () / 1000;

        // USB hotplug: pick up a pendrive inserted after boot and switch to it.
        if (m_USBHCI.UpdatePlugAndPlay ())
        {
            if (f_mount (&m_FileSystemUSB, "USB:", 1) == FR_OK && ScanPhotos ("USB:") > 0)
            {
                m_Logger.Write (FromKernel, LogNotice, "USB hotplug: switched to %u photos",
                                m_PhotoSource.count ());
                m_Photo.set_source (&m_PhotoSource);
            }
        }

        int nCur = m_Scheduler.current ();
        if (nCur >= 0 && m_Plugins[nCur]->wants_continuous_redraw ())
        {
            m_Plugins[nCur]->update ();

            unsigned r0 = CTimer::GetClockTicks ();
            m_Plugins[nCur]->render (m_Canvas);
            unsigned r1 = CTimer::GetClockTicks ();

            // TEMP color-order test: labeled pure-colour swatches (top-right). The letter says
            // what the swatch SHOULD be; report what colour you actually see.
            unsigned sx = m_Canvas.width () - 4 * 44 - 12;
            m_Canvas.fill_rect (sx,       8, 38, 24, lf::rgb::Red);
            m_Canvas.fill_rect (sx + 44,  8, 38, 24, lf::rgb::Green);
            m_Canvas.fill_rect (sx + 88,  8, 38, 24, lf::rgb::Blue);
            m_Canvas.fill_rect (sx + 132, 8, 38, 24, lf::rgb::White);
            m_Canvas.text (sx + 14,  36, "R", lf::rgb::White);
            m_Canvas.text (sx + 58,  36, "G", lf::rgb::White);
            m_Canvas.text (sx + 102, 36, "B", lf::rgb::White);
            m_Canvas.text (sx + 146, 36, "W", lf::rgb::Yellow);

            m_Canvas.present ();
            unsigned r2 = CTimer::GetClockTicks ();

            // Accumulate frame timings.
            unsigned rus = r1 - r0, pus = r2 - r1;
            nFrames++;
            nRenderSum += rus; if (rus > nRenderMax) nRenderMax = rus;
            nPresentSum += pus; if (pus > nPresentMax) nPresentMax = pus;
            if (m_Photo.is_transitioning ()) nFadeFrames++;

            // Log per-photo load stats (decode + downscale) when a new photo was decoded.
            lf::PhotoFramePlugin::LoadStats ls;
            if (m_Photo.take_load_stats (ls))
            {
                m_Logger.Write (FromKernel, LogNotice,
                    "load: photo=%d bytes=%u orig=%ux%u work=%ux%u decode=%ums scale=%ums",
                    ls.index, ls.jpeg_bytes, ls.orig_w, ls.orig_h, ls.work_w, ls.work_h,
                    ls.decode_ms, ls.scale_ms);
            }

            // Log per-second frame-timing aggregate.
            unsigned nWin = m_ElapsedMs - nStatStartMs;
            if (nWin >= 1000 && nFrames > 0)
            {
                m_Logger.Write (FromKernel, LogNotice,
                    "perf: fps=%u frame_avg=%uus render_avg=%uus render_max=%uus "
                    "present_avg=%uus present_max=%uus fade_frames=%u",
                    nFrames * 1000 / nWin, (nRenderSum + nPresentSum) / nFrames,
                    nRenderSum / nFrames, nRenderMax, nPresentSum / nFrames, nPresentMax,
                    nFadeFrames);
                nStatStartMs = m_ElapsedMs;
                nFrames = nFadeFrames = 0;
                nRenderSum = nRenderMax = nPresentSum = nPresentMax = 0;
            }
        }

        unsigned nDurMs = (nCur >= 0) ? m_Scheduler.at (nCur).duration_s * 1000u : 1000u;
        if (m_ElapsedMs - nSwitchAtMs >= nDurMs)
        {
            if (nCur >= 0)
            {
                m_Plugins[nCur]->on_deactivate ();
            }
            Activate (m_Scheduler.next (kNowMin));
            nSwitchAtMs = m_ElapsedMs;
        }
    }

    return ShutdownHalt;
}
