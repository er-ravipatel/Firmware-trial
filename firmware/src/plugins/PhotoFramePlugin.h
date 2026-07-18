// PhotoFramePlugin — the photo-frame screen (plugin #1). For now it draws a placeholder
// "framed photo" that cycles a theme on each activation, standing in for real decoded
// images until the JPEG pipeline lands (Spike C2).
#pragma once
#include "../display/ScreenPlugin.h"

namespace lf {

class PhotoFramePlugin : public ScreenPlugin {
public:
    const char* id() const override { return "photo"; }
    void on_activate() override { index_ = (index_ + 1) % kCount; }
    void render(ICanvas& canvas) override;

private:
    static constexpr int kCount = 3;
    int index_ = -1;   // first on_activate() -> 0 ("Photo 1/3")
};

}  // namespace lf
