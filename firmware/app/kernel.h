//
// kernel.h — Lumen Frame top-level kernel (bare-metal Circle app).
//
// For now this just brings up the framebuffer and draws a boot splash — proving *our*
// firmware boots (not Circle's sample). It will grow into the render engine that hosts the
// ScreenPlugins (see docs/IMPLEMENTATION.md §3).
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/types.h>

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
    void DrawSplash (void);
    void Print (const char *pString);
    void FillRect (unsigned x, unsigned y, unsigned w, unsigned h, TScreenColor color);
    void DrawBorder (TScreenColor color);

    // do not change this order
    CActLED            m_ActLED;
    CKernelOptions     m_Options;
    CDeviceNameService m_DeviceNameService;
    CScreenDevice      m_Screen;
};

#endif
