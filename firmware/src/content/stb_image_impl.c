// stb_image implementation unit, configured for bare-metal: memory-only (no stdio),
// JPEG only, no float/HDR paths. Uses malloc/free/realloc + mem* which Circle provides.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_JPEG
#define STBI_ASSERT(x) ((void) 0)
#include "stb_image.h"   // from firmware/vendor/stb (via EXTRAINCLUDE)
