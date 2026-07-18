// IPhotoSource — device-neutral supplier of photos (JPEG bytes) for the PhotoFrame plugin.
// The firmware backs this with an SD/FatFs implementation; the plugin stays hardware-agnostic.
#pragma once
#include <stdint.h>

namespace lf {

class IPhotoSource {
public:
    virtual ~IPhotoSource() = default;

    // Number of photos available.
    virtual unsigned count() const = 0;

    // Return JPEG bytes for photo `index` (and its length in `len`). The returned buffer is
    // owned by the source and valid until the next jpeg() call. Returns nullptr on failure.
    virtual const uint8_t* jpeg(unsigned index, unsigned& len) = 0;

    // True if `index` is an on-device-undecodable file (HEIC/…) that needs phone-side conversion.
    // The plugin renders a "scan to convert" QR slide for these instead of decoding them.
    virtual bool needs_convert(unsigned index) const { (void) index; return false; }

    // File name (no path) of `index`, for the QR slide / conversion web page. "" if unavailable.
    virtual const char* name(unsigned index) const { (void) index; return ""; }
};

}  // namespace lf
