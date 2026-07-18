// DecodeCore — dedicates CPU core 1 to decoding the next photo in the background, so the main
// render loop on core 0 never stalls on a JPEG decode/scale (which cost 0.4-0.9 s and used to
// freeze the animation once per photo). Cores 2 and 3 are left idle.
//
// Requires ARM_ALLOW_MULTI_CORE (set in vendor/circle/Config.mk). The worker only ever touches
// the plugin's next_/next_bg_ buffers while a job is in flight; the cross-core handshake and
// memory ordering live in PhotoFramePlugin (GCC atomic builtins).
#ifndef _decodecore_h
#define _decodecore_h

#include <circle/multicore.h>
#include <circle/memory.h>
#include <circle/timer.h>
#include "plugins/PhotoFramePlugin.h"   // via EXTRAINCLUDE=-I../src

class CDecodeCore : public CMultiCoreSupport
{
public:
    CDecodeCore (lf::PhotoFramePlugin *pPhoto, CMemorySystem *pMemory)
    :   CMultiCoreSupport (pMemory),
        m_pPhoto (pPhoto)
    {
    }

    // Entry point for each secondary core (1..3). Core 1 becomes the background decoder; the
    // others return immediately (Circle halts a core whose Run() returns).
    void Run (unsigned nCore) override
    {
        if (nCore != 1)
        {
            return;
        }
        for (;;)
        {
            if (!m_pPhoto->bg_service ())
            {
                CTimer::SimpleMsDelay (2);   // idle nap: avoid a 100% busy-spin between jobs
            }
        }
    }

private:
    lf::PhotoFramePlugin *m_pPhoto;
};

#endif
