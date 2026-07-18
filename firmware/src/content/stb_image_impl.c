#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

// --- Fixed decode pool ---------------------------------------------------------------------
// Circle's heap does NOT reclaim freed blocks larger than 512 KB, so repeatedly malloc/free-ing
// multi-MB decode buffers leaks the heap until "Out of memory". Instead, stb allocates from one
// big pool (grabbed once from the heap) that we RESET before every decode. Frees are no-ops.
#define LF_POOL_SIZE (96u * 1024u * 1024u)   // handles up to ~30 MP decodes

static uint8_t* g_pool = 0;
static size_t   g_pool_off = 0;

static void lf_pool_init(void) {
    if (g_pool == 0) {
        g_pool = (uint8_t*) malloc(LF_POOL_SIZE);   // once; intentionally never freed
    }
}

void lf_pool_reset(void) {   // called (extern "C") before each decode
    lf_pool_init();
    g_pool_off = 0;
}

void* lf_pool_alloc(size_t n) {
    lf_pool_init();
    if (g_pool == 0) return 0;
    n = (n + 15u) & ~((size_t) 15u);            // 16-byte align
    if (g_pool_off + n > LF_POOL_SIZE) return 0; // too big -> decode fails gracefully
    uint8_t* p = g_pool + g_pool_off;
    g_pool_off += n;
    for (size_t i = 0; i < n; ++i) p[i] = 0;    // zero (bare-metal RAM is garbage)
    return p;
}

// realloc is not used by the baseline-JPEG path; provide a safe-ish bump version anyway.
static void* lf_pool_realloc(void* p, size_t n) { (void) p; return lf_pool_alloc(n); }

#define STBI_MALLOC(sz)        lf_pool_alloc(sz)
#define STBI_REALLOC(p, newsz) lf_pool_realloc((p), (newsz))
#define STBI_FREE(p)           ((void) (p))

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_JPEG
#define STBI_NO_SIMD          // scalar IDCT: NEON path can fault on bare-metal A53
#define STBI_NO_THREAD_LOCALS // bare metal has no TLS; __thread reads tpidr_el0 -> data abort
#define STBI_ASSERT(x) ((void) 0)
#include "stb_image.h"   // from firmware/vendor/stb (via EXTRAINCLUDE)
