//
// kernel.h — Lumen Frame top-level kernel (bare-metal Circle app).
//
// Brings up the framebuffer + SD/FatFs, then runs the modular display: a PluginScheduler
// drives ScreenPlugins (currently the photo slideshow) onto the CircleCanvas.
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/2dgraphics.h>
#include <circle/types.h>
#include <circle/memory.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>

#include "CircleCanvas.h"
#include "FileLogDevice.h"
#include "SdPhotoSource.h"
#include "DecodeCore.h"
#include "app/PluginScheduler.h"       // via EXTRAINCLUDE=-I../src
#include "plugins/PhotoFramePlugin.h"

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
    void RunSplashIntro (void);              // photo-hero boot splash (fades into the first photo)
    void DrawWordmark (unsigned nAlpha);     // centered LUMEN FRAME + accent + version, alpha 0..255
    unsigned ScanPhotos (const char *pDrive);   // scans photos/ then images/ then root
    // Read a boolean key from SD:/lumen.conf (key=on/off/1/0/true/false). Returns bDefault when
    // the file or key is absent. Used to keep diagnostics OFF in production builds by default.
    boolean ReadConfigFlag (const char *pKey, boolean bDefault);

    // do not change this order
    CActLED            m_ActLED;
    CKernelOptions     m_Options;
    CDeviceNameService m_DeviceNameService;
    CSerialDevice      m_Serial;
    CExceptionHandler  m_ExceptionHandler;
    CInterruptSystem   m_Interrupt;
    CTimer             m_Timer;
    CLogger            m_Logger;
    CUSBHCIDevice      m_USBHCI;
    CEMMCDevice        m_EMMC;
    C2DGraphics        m_Graphics;

    CircleCanvas       m_Canvas;
    FATFS              m_FileSystemSD;
    FATFS              m_FileSystemUSB;
    CFileLogDevice     m_FileLog;
    CSdPhotoSource     m_PhotoSource;
    u32                m_ElapsedMs;     // animation clock (ms since boot)

    lf::PluginScheduler<8> m_Scheduler;
    lf::PhotoFramePlugin   m_Photo;
    CDecodeCore            m_DecodeCore;   // core-1 background JPEG decoder (must follow m_Photo)
    lf::ScreenPlugin      *m_Plugins[8];
    unsigned               m_PluginCount;
};

#endif
