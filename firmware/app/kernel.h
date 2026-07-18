//
// kernel.h — Lumen Frame top-level kernel (bare-metal Circle app).
//
// Brings up the framebuffer and runs the modular display: a PluginScheduler rotates
// ScreenPlugins (photo, clock, ...) onto the CircleCanvas. See docs/IMPLEMENTATION.md.
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/2dgraphics.h>
#include <circle/types.h>

#include "CircleCanvas.h"
#include "app/PluginScheduler.h"       // via EXTRAINCLUDE=-I../src
#include "plugins/PhotoFramePlugin.h"
#include "plugins/ClockPlugin.h"

enum TShutdownMode
{
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
};

class CKernel
{
public:
    CKernel (void);
    ~CKernel (void);

    boolean Initialize (void);
    TShutdownMode Run (void);

private:
    void SetupPlugins (void);
    void Activate (int nIndex);

    // do not change this order
    CActLED            m_ActLED;
    CKernelOptions     m_Options;
    CDeviceNameService m_DeviceNameService;
    C2DGraphics        m_Graphics;

    // Display stack (m_Graphics must be constructed before m_Canvas).
    CircleCanvas       m_Canvas;
    u32                m_ElapsedMs;     // before m_Clock (which holds &m_ElapsedMs)

    lf::PluginScheduler<8> m_Scheduler;
    lf::PhotoFramePlugin   m_Photo;
    lf::ClockPlugin        m_Clock;
    lf::ScreenPlugin      *m_Plugins[8];
    unsigned               m_PluginCount;
};

#endif
