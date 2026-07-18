//
// kernel.cpp — Lumen Frame modular display loop (with SD/FatFs photo loading).
//
#include "kernel.h"
#include <circle/timer.h>

static const char FromKernel[] = "kernel";

// Fake time-of-day for scheduling until an RTC/NTP source exists (10:00).
static const unsigned kNowMin = 600;
static const unsigned kTickMs = 100;

CKernel::CKernel (void)
:   m_Timer (&m_Interrupt),
    m_Logger (m_Options.GetLogLevel (), &m_Timer),
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

    if (bOK) bOK = m_Serial.Initialize (115200);
    if (bOK) bOK = m_Logger.Initialize (&m_Serial);
    if (bOK) bOK = m_Interrupt.Initialize ();
    if (bOK) bOK = m_Timer.Initialize ();
    if (bOK) bOK = m_EMMC.Initialize ();
    if (bOK) bOK = m_Graphics.Initialize ();

    return bOK;
}

void CKernel::SetupPlugins (void)
{
    m_Plugins[0] = &m_Photo;
    m_Scheduler.add ({"photo", true, 2, -1, -1});

    m_Plugins[1] = &m_Clock;
    m_Scheduler.add ({"clock", true, 2, -1, -1});

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

TShutdownMode CKernel::Run (void)
{
    // Mount the SD card and scan for photos. On any failure the PhotoFrame plugin falls
    // back to its embedded image.
    if (f_mount (&m_FileSystem, "SD:", 1) == FR_OK)
    {
        m_PhotoSource.Scan ("SD:/");
        if (m_PhotoSource.count () > 0)
        {
            m_Logger.Write (FromKernel, LogNotice, "Found %u photo(s) on SD", m_PhotoSource.count ());
            m_Photo.set_source (&m_PhotoSource);
        }
        else
        {
            m_Logger.Write (FromKernel, LogWarning, "No *.jpg on SD; using embedded image");
        }
    }
    else
    {
        m_Logger.Write (FromKernel, LogWarning, "Could not mount SD; using embedded image");
    }

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
