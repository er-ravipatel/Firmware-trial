// version.h — single source of truth for the Lumen Frame build identity.
//
// If LUMEN_VERSION contains "beta" or "dev" (case-insensitive) the build is treated as a
// developer/testing build: diagnostic logging is FORCED ON regardless of SD:/lumen.conf, so a
// dev build is never accidentally silent. A release string (e.g. "1.0.0") obeys the config flag.
#ifndef _version_h
#define _version_h

#define LUMEN_VERSION "0.2.0-beta"

// Operating mode shown on the splash. Static for now (no networking yet); becomes dynamic when
// WiFi/online features land.
#define LUMEN_MODE    "Offline-only"

// Build stamp: the compiler's build date/time (updates automatically every build). Swap for a
// manual incrementing number here if you prefer a plain build count.
#define LUMEN_BUILD   __DATE__

#endif
