// PhotoFramePlugin — a self-advancing photo slideshow with cross-fade transitions.
// Photos come from an IPhotoSource (e.g. SD card); each is decoded (EXIF-corrected in the
// decoder), scaled to fit the frame with letterboxing into a prepared buffer, then dissolved
// into the next. Falls back to an embedded image if no source/photos are available.
#pragma once
#include <stdint.h>
#include "../display/ScreenPlugin.h"
#include "../content/IPhotoSource.h"

namespace lf {

class PhotoFramePlugin : public ScreenPlugin {
public:
    const char* id() const override { return "photo"; }
    bool wants_continuous_redraw() const override { return true; }
    void on_activate() override;
    void render(ICanvas& canvas) override;

    void set_source(IPhotoSource* source) { source_ = source; }
    void set_clock(const uint32_t* elapsed_ms) { ms_ = elapsed_ms; }

private:
    enum class State { Empty, Showing, Fading };

    void ensure_buffers(unsigned fw, unsigned fh);
    void prepare(int photo_index, uint8_t* dst);      // decode+scale photo into dst (frame size)
    void draw_frame(ICanvas& canvas, unsigned fx, unsigned fy);

    unsigned now() const { return ms_ ? *ms_ : 0; }
    unsigned photo_count() const { return source_ ? source_->count() : 0; }

    static const unsigned kDwellMs = 3000;   // time each photo is held
    static const unsigned kFadeMs = 800;     // cross-fade duration

    IPhotoSource* source_ = nullptr;
    const uint32_t* ms_ = nullptr;

    int index_ = -1;
    State state_ = State::Empty;
    unsigned phase_start_ = 0;

    // Prepared frame-sized buffers (frame area WxH), letterboxed.
    uint8_t* cur_ = nullptr;
    uint8_t* next_ = nullptr;
    unsigned fw_ = 0, fh_ = 0;   // prepared buffer dimensions
};

}  // namespace lf
