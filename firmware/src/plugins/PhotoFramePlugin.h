// PhotoFramePlugin — photo slideshow with Ken Burns (slow zoom/pan) and cross-fade transitions.
// Each frame is sampled live from the decoded source image (bilinear) with a time-animated
// zoom/pan transform, so quality is good and motion is smooth. Two photos are rendered and
// blended during the dissolve. Falls back to an embedded image if no source/photos exist.
#pragma once
#include <stdint.h>
#include "../display/ScreenPlugin.h"
#include "../content/IPhotoSource.h"
#include "../content/JpegDecoder.h"

namespace lf {

class PhotoFramePlugin : public ScreenPlugin {
public:
    const char* id() const override { return "photo"; }
    bool wants_continuous_redraw() const override { return true; }
    void on_activate() override {
        if (state_ == State::Fading) state_ = State::Showing;  // don't resume a mid-fade
        photo_start_ = now();                                  // fresh dwell on re-entry
    }
    void render(ICanvas& canvas) override;

    void set_source(IPhotoSource* source) { source_ = source; reset(); }
    void set_clock(const uint32_t* elapsed_ms) { ms_ = elapsed_ms; }

    // Restart the slideshow (e.g. after the photo source changes at runtime).
    void reset();

private:
    enum class State { Empty, Showing, Fading };

    void ensure_buffers(unsigned fw, unsigned fh);
    void load(int idx, DecodedImage& img);
    // Render one Ken Burns frame of `img` at animation progress [0,1] into dst (fw_ x fh_).
    void render_kb(const DecodedImage& img, float progress, int variant, uint8_t* dst);

    unsigned now() const { return ms_ ? *ms_ : 0; }
    unsigned photo_count() const { return source_ ? source_->count() : 0; }
    static int variant_of(int idx) { return idx & 3; }

    static const unsigned kDwellMs = 6000;   // display time per photo (incl. fade-in)
    static const unsigned kFadeMs = 1000;    // cross-fade duration
    static const unsigned kSpanMs = kDwellMs + kFadeMs;  // total on-screen span (KB timeline)

    IPhotoSource* source_ = nullptr;
    const uint32_t* ms_ = nullptr;

    int index_ = -1;
    int cur_variant_ = 0, next_variant_ = 0;
    State state_ = State::Empty;
    unsigned photo_start_ = 0;   // when the current photo first appeared
    unsigned fade_start_ = 0;    // when the current fade began

    DecodedImage cur_{}, next_{};
    uint8_t* fbA_ = nullptr;     // rendered frame for the current/outgoing photo
    uint8_t* fbB_ = nullptr;     // rendered frame for the incoming photo
    unsigned fw_ = 0, fh_ = 0;
};

}  // namespace lf
