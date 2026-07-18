// stb_image implementation unit, configured for bare-metal: memory-only (no stdio),
// JPEG only, no float/HDR paths. Uses malloc/free/realloc + mem* which Circle provides.
#include <stdlib.h>

// Zero-initialise stb's allocations. Bare-metal malloc returns uninitialised memory (real RAM
// has garbage), whereas QEMU zeroes RAM — so an uninitialised read that is harmlessly 0 in the
// emulator becomes a wild pointer on real hardware (data abort). Zeroing makes both behave.
// Manual loop (not memset) to avoid the fortified __memset_chk, which Circle doesn't provide.
static void *stbi_zmalloc (size_t n)
{
    unsigned char *p = (unsigned char *) malloc (n);
    if (p)
    {
        for (size_t i = 0; i < n; ++i) p[i] = 0;
    }
    return p;
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_JPEG
#define STBI_NO_SIMD          // use scalar integer IDCT: NEON path can fault on bare-metal A53
#define STBI_NO_THREAD_LOCALS // bare metal has no TLS; __thread reads tpidr_el0 -> data abort
#define STBI_ASSERT(x) ((void) 0)
#define STBI_MALLOC(sz)        stbi_zmalloc (sz)
#define STBI_REALLOC(p, newsz) realloc ((p), (newsz))
#define STBI_FREE(p)           free (p)
#include "stb_image.h"   // from firmware/vendor/stb (via EXTRAINCLUDE)
