// PhotoFramePlugin — the photo-frame screen (plugin #1). Decodes and displays a JPEG.
// Source is either an externally supplied buffer (e.g. loaded from SD) via set_jpeg(),
// or an embedded fallback image. Falls back to a placeholder if decode fails.
#pragma once
#include <stdint.h>
#include "../display/ScreenPlugin.h"
#include "../content/JpegDecoder.h"

namespace lf {

class PhotoFramePlugin : public ScreenPlugin {
public:
    const char* id() const override { return "photo"; }
    void on_activate() override { index_ = (index_ + 1) % kCount; }
    void render(ICanvas& canvas) override;

    // Provide JPEG bytes from an external source (e.g. SD card). Overrides the embedded image.
    void set_jpeg(const uint8_t* data, unsigned len) {
        jpeg_ = data;
        jpeg_len_ = len;
        tried_ = false;
        img_ = DecodedImage{};
    }

private:
    void ensure_decoded();

    static constexpr int kCount = 3;
    int index_ = -1;                 // first on_activate() -> 0 ("Photo 1/3")
    bool tried_ = false;             // decode attempted?
    const uint8_t* jpeg_ = nullptr;  // external source; nullptr => embedded fallback
    unsigned jpeg_len_ = 0;
    DecodedImage img_{};
};

}  // namespace lf
