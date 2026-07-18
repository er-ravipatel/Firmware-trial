// Minimal libc shims for vendored freestanding C code. Circle's environment does not provide
// abs()/labs() from <stdlib.h>, which the qrcodegen library calls.
int abs (int x)    { return x < 0 ? -x : x; }
long labs (long x) { return x < 0 ? -x : x; }
