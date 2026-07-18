//
// kernel.cpp — Lumen Frame modular display loop (with SD/FatFs photo loading).
//
#include "kernel.h"
#include "version.h"
#include <circle/timer.h>
#include <circle/string.h>
#include <circle/font.h>
#include <qrcodegen.h>   // Nayuki QR encoder (extern "C"); via EXTRAINCLUDE=-I../vendor/qrcodegen

static const char FromKernel[] = "kernel";

// Fake time-of-day for scheduling until an RTC/NTP source exists (10:00).
static const unsigned kNowMin = 600;

// SoftAP (portal mode) config. CYW43 firmware blobs must be on the card at this path.
#define FIRMWARE_PATH  "SD:/firmware/"
#define AP_SSID        "LumenFrame"
#define AP_CHANNEL     6
static const u8 s_APIP[]   = {192, 168, 1, 1};
static const u8 s_APMask[] = {255, 255, 255, 0};

// --- Tiny config-file helpers (freestanding: no libc strcmp/tolower) ---
static inline char lc (char c) { return (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c; }

static boolean streq (const char *a, const char *b)
{
    while (*a && *b) { if (*a != *b) return FALSE; a++; b++; }
    return *a == *b;
}

// Case-insensitive substring test: does pHay contain pNeedle anywhere?
static boolean containsCI (const char *pHay, const char *pNeedle)
{
    for (const char *h = pHay; *h; ++h)
    {
        const char *a = h;
        const char *b = pNeedle;
        while (*a && *b && lc (*a) == lc (*b)) { a++; b++; }
        if (*b == '\0') return TRUE;   // reached end of needle -> matched
    }
    return FALSE;
}

// A beta/dev build forces logging on (see version.h).
static boolean VersionForcesLog (void)
{
    return containsCI (LUMEN_VERSION, "beta") || containsCI (LUMEN_VERSION, "dev");
}

// 8-bit channel interpolation for the splash gradient (u in 0..255).
static inline u8 lerp8 (int a, int b, int u) { return (u8) (a + (b - a) * u / 255); }

// Parse a trimmed value token ("on"/"off"/"1"/"0"/"true"/"false"/"yes"/"no").
static boolean ParseBool (const char *v, boolean bDefault)
{
    char w[8];
    unsigned n = 0;
    while (v[n] && v[n] != ' ' && v[n] != '\t' && v[n] != '\r' && v[n] != '\n' && v[n] != '#' && n < 7)
    {
        w[n] = lc (v[n]);
        n++;
    }
    w[n] = '\0';
    if (streq (w, "on") || streq (w, "1") || streq (w, "true") || streq (w, "yes")) return TRUE;
    if (streq (w, "off") || streq (w, "0") || streq (w, "false") || streq (w, "no")) return FALSE;
    return bDefault;
}

CKernel::CKernel (void)
:   m_Timer (&m_Interrupt),
    m_Logger (m_Options.GetLogLevel (), &m_Timer),
    m_USBHCI (&m_Interrupt, &m_Timer, TRUE),   // TRUE = enable plug-and-play (USB hotplug)
    m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
    m_Graphics (m_Options.GetWidth (), m_Options.GetHeight (), FALSE),
    m_Canvas (m_Graphics),
    m_WLAN (FIRMWARE_PATH),
    m_Net (s_APIP, s_APMask, 0, 0, "lumen", NetDeviceTypeWLAN),
    m_ElapsedMs (0),
    m_DecodeCore (&m_Photo, CMemorySystem::Get ()),   // core-1 background decoder
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

    // Start the secondary cores LAST (Circle requirement). Core 1 becomes the background photo
    // decoder; a failure is non-fatal (the plugin falls back to inline decode).
    if (bOK && !m_DecodeCore.Initialize ())
    {
        m_Logger.Write (FromKernel, LogWarning, "multicore init failed; decode will run inline");
    }

    return bOK;
}

void CKernel::SetupPlugins (void)
{
    m_Photo.set_clock (&m_ElapsedMs);          // drives dwell + cross-fade timing
    m_Photo.set_time_us (&CTimer::GetClockTicks);   // microsecond clock for perf logging

    m_Plugins[0] = &m_Photo;
    m_Scheduler.add ({"photo", true, 90, -1, -1});   // slideshow is the only screen

    m_PluginCount = 1;
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

// Read a boolean key from SD:/lumen.conf. Format: one "key = value" per line, '#' comments.
// Returns bDefault when the file is missing or the key is not found.
boolean CKernel::ReadConfigFlag (const char *pKey, boolean bDefault)
{
    FIL File;
    if (f_open (&File, "SD:/lumen.conf", FA_READ) != FR_OK)
    {
        return bDefault;   // no config file -> default (production ships without one)
    }
    char Buf[512];
    UINT nRead = 0;
    if (f_read (&File, Buf, sizeof (Buf) - 1, &nRead) != FR_OK) nRead = 0;
    f_close (&File);
    Buf[nRead] = '\0';

    unsigned nKeyLen = 0;
    while (pKey[nKeyLen]) nKeyLen++;

    const char *p = Buf;
    while (*p)
    {
        while (*p == ' ' || *p == '\t') p++;          // skip leading indent
        if (*p == '#')                                // comment -> skip line
        {
            while (*p && *p != '\n') p++;
        }
        else
        {
            boolean bMatch = TRUE;                     // case-insensitive key compare
            for (unsigned i = 0; i < nKeyLen; i++)
            {
                if (lc (p[i]) != lc (pKey[i])) { bMatch = FALSE; break; }
            }
            if (bMatch)
            {
                const char *q = p + nKeyLen;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '=')
                {
                    q++;
                    while (*q == ' ' || *q == '\t') q++;
                    return ParseBool (q, bDefault);
                }
            }
            while (*p && *p != '\n') p++;              // advance to next line
        }
        if (*p) p++;
    }
    return bDefault;
}

// Centered wordmark ("LUMEN FRAME"), a thin cyan accent rule, and the version — drawn on top of
// whatever background is already in the back buffer, dimmed by nAlpha (0=invisible..255=full) so
// it can fade in/out during the splash.
void CKernel::DrawWordmark (unsigned nAlpha)
{
    if (nAlpha > 255) nAlpha = 255;
    const unsigned W = m_Canvas.width ();
    const unsigned H = m_Canvas.height ();
    const unsigned cx = W / 2;

    // Brand wordmark.
    T2DColor White = COLOR2D (232 * nAlpha / 255, 238 * nAlpha / 255, 246 * nAlpha / 255);
    m_Graphics.DrawText (cx, H / 2 - 52, White, "LUMEN FRAME",
                         C2DGraphics::AlignCenter, Font12x22);

    // Thin warm-gold accent rule under the wordmark (harmonizes with the wine/plum gradient).
    const unsigned bw = 168;
    m_Canvas.fill_rect (cx - bw / 2, H / 2 - 22, bw, 2,
                        lf::Rgb { (u8) (232 * nAlpha / 255), (u8) (196 * nAlpha / 255),
                                  (u8) (138 * nAlpha / 255) });

    // Tagline.
    T2DColor Soft = COLOR2D (202 * nAlpha / 255, 212 * nAlpha / 255, 226 * nAlpha / 255);
    m_Graphics.DrawText (cx, H / 2 - 8, Soft, "Memory Lane Walkthrough",
                         C2DGraphics::AlignCenter, Font8x16);

    // Credits (the major attribution line).
    T2DColor Warm = COLOR2D (176 * nAlpha / 255, 184 * nAlpha / 255, 196 * nAlpha / 255);
    m_Graphics.DrawText (cx, H / 2 + 20, Warm,
                         "Thought by Vikash   &   Guided by Ravi   &   Written by Claude",
                         C2DGraphics::AlignCenter, Font8x14);

    // Build + mode, small, in the bottom-right corner.
    CString Meta;
    Meta.Format ("v%s   |   %s   |   build %s", LUMEN_VERSION, LUMEN_MODE, LUMEN_BUILD);
    T2DColor Dim = COLOR2D (118 * nAlpha / 255, 130 * nAlpha / 255, 144 * nAlpha / 255);
    m_Graphics.DrawText (W - 14, H - 18, Dim, (const char *) Meta,
                         C2DGraphics::AlignRight, Font6x7);
}

// Boot splash: a rich, opaque diagonal gradient (modern web-hero style) is the background, with
// the wordmark + tagline + credits on top. It covers the (invisible) SD mount and first-photo
// decode, holds for the readable dwell, then dissolves straight into the live slideshow. No boot
// log on screen.
void CKernel::RunSplashIntro (void)
{
    const unsigned W = m_Canvas.width ();
    const unsigned H = m_Canvas.height ();

    m_Photo.intro_alloc (W, H);              // allocate the plugin's fullscreen buffers (fast)
    u8 *pGrad = m_Photo.intro_scratch ();    // reuse the spare frame buffer for the gradient

    // Build a rich diagonal 3-stop gradient once (top-left -> bottom-right): deep indigo -> wine
    // magenta (a bright diagonal band) -> dark plum, with a subtle dither so it stays band-free.
    for (unsigned y = 0; y < H; y++)
    {
        int ty = (H > 1) ? (int) (y * 255 / (H - 1)) : 0;
        u8 *pRow = pGrad + (unsigned long) y * W * 3;
        for (unsigned x = 0; x < W; x++)
        {
            int tx = (W > 1) ? (int) (x * 255 / (W - 1)) : 0;
            int t = (tx + ty) / 2;
            int rr, gg, bb;
            if (t < 128)
            {
                int u = t * 2;                              // deep indigo -> wine magenta
                rr = lerp8 (28, 132, u); gg = lerp8 (18, 38, u); bb = lerp8 (54, 82, u);
            }
            else
            {
                int u = (t - 128) * 2;                      // wine magenta -> dark plum
                rr = lerp8 (132, 34, u); gg = lerp8 (38, 16, u); bb = lerp8 (82, 46, u);
            }
            int d = (int) ((x * 13 + y * 7) & 7) - 4;       // +/-4 dither to break banding
            rr += d; gg += d; bb += d;
            if (rr < 0) rr = 0; else if (rr > 255) rr = 255;
            if (gg < 0) gg = 0; else if (gg > 255) gg = 255;
            if (bb < 0) bb = 0; else if (bb > 255) bb = 255;
            pRow[x * 3] = (u8) rr; pRow[x * 3 + 1] = (u8) gg; pRow[x * 3 + 2] = (u8) bb;
        }
    }

    // Phase A: fade the wordmark in over the gradient (~600 ms).
    unsigned nStart = CTimer::GetClockTicks () / 1000;
    for (;;)
    {
        unsigned e = CTimer::GetClockTicks () / 1000 - nStart;
        if (e >= 600) break;
        m_Canvas.blit_rgb (0, 0, W, H, pGrad);
        DrawWordmark (e * 255 / 600);
        m_Canvas.present ();
    }

    // Hold the full-brightness gradient splash while the first photo decodes (blocks ~0.5-0.9 s).
    m_Canvas.blit_rgb (0, 0, W, H, pGrad);
    DrawWordmark (255);
    m_Canvas.present ();
    m_Photo.intro_load ();
    const u8 *pSharp = m_Photo.intro_sharp ();   // sharp first slideshow frame

    // Phase C: hold the gradient + wordmark + credits for 10 s (readable, static frame).
    m_Canvas.blit_rgb (0, 0, W, H, pGrad);
    DrawWordmark (255);
    m_Canvas.present ();
    CTimer::SimpleMsDelay (10000);

    // Phase D: dissolve the gradient into the first photo (~1000 ms) as the wordmark fades out.
    nStart = CTimer::GetClockTicks () / 1000;
    for (;;)
    {
        unsigned e = CTimer::GetClockTicks () / 1000 - nStart;
        if (e >= 1000) break;
        m_Canvas.blit_rgb_blend (0, 0, W, H, pGrad, pSharp, e * 256 / 1000);
        DrawWordmark (255 - e * 255 / 1000);
        m_Canvas.present ();
    }
}

// Encode pText as a QR and draw it centered at (cx,cy). Returns the drawn side length in px.
unsigned CKernel::DrawQR (const char *pText, unsigned cx, unsigned cy, unsigned nModulePx)
{
    static const int kMaxVer = 8;   // ample for a short Wi-Fi payload; keeps the stack buffers small
    u8 qr[qrcodegen_BUFFER_LEN_FOR_VERSION (kMaxVer)];
    u8 tmp[qrcodegen_BUFFER_LEN_FOR_VERSION (kMaxVer)];
    if (!qrcodegen_encodeText (pText, tmp, qr, qrcodegen_Ecc_MEDIUM,
                               qrcodegen_VERSION_MIN, kMaxVer, qrcodegen_Mask_AUTO, true))
    {
        return 0;
    }

    int n = qrcodegen_getSize (qr);
    const unsigned quiet = 4;                                   // quiet zone (modules)
    unsigned dim = (unsigned) (n + 2 * (int) quiet) * nModulePx;
    unsigned x0 = cx - dim / 2;
    unsigned y0 = cy - dim / 2;

    m_Canvas.fill_rect (x0, y0, dim, dim, lf::Rgb {255, 255, 255});   // white incl. quiet zone
    for (int my = 0; my < n; my++)
    {
        for (int mx = 0; mx < n; mx++)
        {
            if (qrcodegen_getModule (qr, mx, my))
            {
                m_Canvas.fill_rect (x0 + (quiet + (unsigned) mx) * nModulePx,
                                    y0 + (quiet + (unsigned) my) * nModulePx,
                                    nModulePx, nModulePx, lf::Rgb {0, 0, 0});
            }
        }
    }
    return dim;
}

// Draw the full portal/conversion screen: title, the Wi-Fi-join QR, and instructions. No hardware
// touched, so it is also used by the QEMU `qrtest` path.
void CKernel::DrawPortalScreen (void)
{
    const unsigned W = m_Canvas.width ();
    const unsigned H = m_Canvas.height ();

    m_Canvas.clear (lf::rgb::Navy);

    // Adaptive module size (display-agnostic): QR ≈ 40% of screen height.
    unsigned nMod = H / 92;
    if (nMod < 4) nMod = 4;
    if (nMod > 12) nMod = 12;

    unsigned qcy = H / 2 + 8;
    unsigned dim = DrawQR ("WIFI:S:" AP_SSID ";T:nopass;;", W / 2, qcy, nMod);
    if (dim == 0) return;
    unsigned qtop = qcy - dim / 2;
    unsigned qbot = qcy + dim / 2;

    m_Graphics.DrawText (W / 2, qtop - 46, COLOR2D (232, 238, 246), "LUMEN FRAME",
                         C2DGraphics::AlignCenter, Font12x22);
    m_Graphics.DrawText (W / 2, qtop - 16, COLOR2D (200, 210, 224), "Photo conversion",
                         C2DGraphics::AlignCenter, Font8x16);
    m_Graphics.DrawText (W / 2, qbot + 22, COLOR2D (232, 196, 138), "Scan with your phone camera",
                         C2DGraphics::AlignCenter, Font8x16);
    m_Graphics.DrawText (W / 2, qbot + 46, COLOR2D (180, 190, 205),
                         "or join Wi-Fi \"" AP_SSID "\"  (no password)",
                         C2DGraphics::AlignCenter, Font8x14);
    m_Canvas.present ();
}

// Portal mode: bring up the SoftAP + DHCP/DNS/HTTP and serve the setup page. Blocks forever
// (runs the Circle scheduler so the net tasks get CPU). Entered on demand; the slideshow is not
// running here, so networking has the machine to itself. Returns only on bring-up failure path.
void CKernel::RunPortalMode (void)
{
    const unsigned W = m_Canvas.width ();
    const unsigned H = m_Canvas.height ();

    m_Logger.Write (FromKernel, LogNotice, "Portal mode: starting SoftAP '%s'", AP_SSID);

    boolean bOK = m_WLAN.Initialize ();
    if (bOK) bOK = m_WLAN.CreateOpenNet (AP_SSID, AP_CHANNEL, FALSE);
    if (bOK) bOK = m_Net.Initialize ();
    if (!bOK)
    {
        m_Logger.Write (FromKernel, LogError, "Portal: WLAN/net bring-up FAILED");
        m_Canvas.clear (lf::rgb::Navy);
        m_Graphics.DrawText (W / 2, H / 2, COLOR2D (240, 120, 120), "Wi-Fi setup failed - see log",
                             C2DGraphics::AlignCenter, Font8x16);
        m_Canvas.present ();
        for (;;) { m_CoopSched.Yield (); }
    }

    // Net services, as cooperative-scheduler tasks.
    new CDHCPD (&m_Net, s_APIP);
    new CDNSD (&m_Net, s_APIP);
    new CWebServer (&m_Net);

    m_Logger.Write (FromKernel, LogNotice, "Portal up: join Wi-Fi '%s' -> setup page opens", AP_SSID);

    DrawPortalScreen ();

    // Serve forever. (Reboot-to-resume-slideshow after conversion is added with the real trigger.)
    for (;;) { m_CoopSched.Yield (); }
}

TShutdownMode CKernel::Run (void)
{
    const unsigned W = m_Canvas.width ();

    // ---- Set up SD-card file logging FIRST, and retarget the logger to it, so everything
    // below (incl. any Circle panic/exception) is captured in SD:/lumenlog.txt with f_sync.
    // Gated by "logging=on" in SD:/lumen.conf — OFF by default so production frames don't write
    // to the card every line (dev/testing only). A beta/dev build (see version.h) FORCES it on,
    // overriding the config, so a developer build is never silent. Serial logging is unaffected. ----
    boolean bSDMounted = (f_mount (&m_FileSystemSD, "SD:", 1) == FR_OK);
    boolean bForced = VersionForcesLog ();                                   // beta/dev overrides config
    boolean bLogEnabled = bSDMounted && (bForced || ReadConfigFlag ("logging", FALSE));
    boolean bLogOpen = bLogEnabled && m_FileLog.Open ("SD:/lumenlog.txt");
    if (bLogOpen)
    {
        m_Logger.SetNewTarget (&m_FileLog);
    }
    m_Logger.Write (FromKernel, LogNotice,
                    "==== Lumen Frame boot ==== ver=%s SDmount=%d forced=%d log=%d",
                    LUMEN_VERSION, (int) bSDMounted, (int) bForced, (int) bLogOpen);

    // ---- QR render test (QEMU-friendly: draws the portal screen with NO WiFi, then halts). ----
    if (bSDMounted && ReadConfigFlag ("qrtest", FALSE))
    {
        DrawPortalScreen ();
        for (;;) { CTimer::SimpleMsDelay (1000); }
    }

    // ---- Portal mode (config-gated during bring-up; the HEIC-on-pendrive trigger replaces this
    // flag later). When on, serve the Wi-Fi setup page instead of the slideshow. Does not return. ----
    if (bSDMounted && ReadConfigFlag ("portal", FALSE))
    {
        RunPortalMode ();
    }

    // ---- Detect the photo source silently (steps go to the SD log, not the screen — the
    // splash covers boot). Prefer a USB pendrive; fall back to the SD card; then embedded. ----
    unsigned nPhotos = 0;
    const char *pSource = "embedded fallback";

    boolean bUSB = (f_mount (&m_FileSystemUSB, "USB:", 1) == FR_OK);
    unsigned nUSB = bUSB ? ScanPhotos ("USB:") : 0;

    if (nUSB > 0)
    {
        nPhotos = nUSB;
        pSource = "USB pendrive";
        m_Photo.set_source (&m_PhotoSource);
    }
    else if (bSDMounted)
    {
        nPhotos = ScanPhotos ("SD:");
        if (nPhotos > 0)
        {
            pSource = "SD card";
            m_Photo.set_source (&m_PhotoSource);
        }
    }

    m_Logger.Write (FromKernel, LogNotice, "Boot: source=%s photos=%u res=%ux%u",
                    pSource, nPhotos, W, m_Canvas.height ());

    SetupPlugins ();

    // Beautiful photo-hero splash (covers SD mount + first-photo decode, fades into the slideshow).
    RunSplashIntro ();

    unsigned nSwitchAtMs = CTimer::GetClockTicks () / 1000;
    m_ElapsedMs = nSwitchAtMs;
    boolean bUsingUSB = (nUSB > 0);   // track current source for hotplug in/out
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

        // USB hotplug: switch to a pendrive when inserted; fall back to the SD when removed.
        // Decide presence with a name-service lookup (no device I/O). This is the crucial safety
        // point: doing forced-mount I/O (f_mount "...",1) on a just-removed device RACES with
        // Circle's plug-and-play teardown and can data-abort (~1-in-7 crash on removal, seen in
        // the field log). So we only mount/scan on a real *insert* (device confirmed present and
        // fully enumerated), and on *removal* we touch nothing on the vanished device.
        if (m_USBHCI.UpdatePlugAndPlay ())
        {
            boolean bUSBpresent = (m_DeviceNameService.GetDevice ("umsd1", TRUE) != 0);
            if (bUSBpresent && !bUsingUSB)
            {
                // Newly inserted and enumerated -> safe to mount + scan.
                if (f_mount (&m_FileSystemUSB, "USB:", 1) == FR_OK && ScanPhotos ("USB:") > 0)
                {
                    m_Logger.Write (FromKernel, LogNotice, "USB inserted: %u photos",
                                    m_PhotoSource.count ());
                    m_Photo.set_source (&m_PhotoSource);
                    bUsingUSB = TRUE;
                }
            }
            else if (!bUSBpresent && bUsingUSB)
            {
                // Removed -> release the FatFs volume WITHOUT any USB I/O, then fall back to SD.
                f_mount (0, "USB:", 0);
                if (bSDMounted && ScanPhotos ("SD:") > 0)
                {
                    m_Logger.Write (FromKernel, LogNotice, "USB removed: back to SD (%u photos)",
                                    m_PhotoSource.count ());
                    m_Photo.set_source (&m_PhotoSource);
                }
                bUsingUSB = FALSE;
            }
        }

        int nCur = m_Scheduler.current ();
        if (nCur >= 0 && m_Plugins[nCur]->wants_continuous_redraw ())
        {
            m_Plugins[nCur]->update ();

            unsigned r0 = CTimer::GetClockTicks ();
            m_Plugins[nCur]->render (m_Canvas);
            unsigned r1 = CTimer::GetClockTicks ();
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

        // Rotate plugins only when there is more than one; with the photo slideshow as the sole
        // screen, re-activating it would needlessly restart its dwell/Ken Burns.
        if (m_PluginCount > 1)
        {
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
    }

    return ShutdownHalt;
}
