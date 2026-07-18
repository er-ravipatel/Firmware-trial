// PhotoFramePlugin — the photo-frame screen (plugin #1). Decodes and displays a real JPEG
// (embedded for now; SD-card loading next). Falls back to a placeholder if decode fails.
#pragma once
#include "../display/ScreenPlugin.h"
#include "../content/JpegDecoder.h"

namespace lf {

class PhotoFramePlugin : public ScreenPlugin {
public:
    const char* id() const override { return "photo"; }
    void on_activate() override { index_ = (index_ + 1) % kCount; }
    void render(ICanvas& canvas) override;

private:
    void ensure_decoded();

    static constexpr int kCount = 3;
    int index_ = -1;          // first on_activate() -> 0 ("Photo 1/3")
    bool tried_ = false;      // decode attempted?
    DecodedImage img_{};
};

}  // namespace lf
