//
// main.cpp — Lumen Frame firmware entry point.
//
// Standard Circle bring-up: construct the kernel, initialize, run. Control never returns
// to main() normally (the kernel loops); halt/reboot on shutdown.
//
#include "kernel.h"
#include <circle/startup.h>

int main (void)
{
    CKernel Kernel;
    if (!Kernel.Initialize ())
    {
        halt ();
        return EXIT_HALT;
    }

    TShutdownMode ShutdownMode = Kernel.Run ();

    switch (ShutdownMode)
    {
    case ShutdownReboot:
        reboot ();
        return EXIT_REBOOT;

    case ShutdownHalt:
    default:
        halt ();
        return EXIT_HALT;
    }
}
