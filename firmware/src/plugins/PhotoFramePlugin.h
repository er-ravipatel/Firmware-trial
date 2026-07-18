// PhotoFramePlugin — the photo-frame slideshow (plugin #1). Cycles through photos from an
// IPhotoSource (e.g. the SD card), decoding each JPEG and scaling it to fit the frame,
// preserving aspect ratio. Falls back to an embedded image if no source/photos are available.
#pragma once
#include <stdint.h>
#include "../display/ScreenPlugin.h"
#include "../content/IPhotoSource.h"
#include "../content/JpegDecoder.h"

namespace lf {

class PhotoFramePlugin : public ScreenPlugin {
public:
    const char* id() const override { return "photo"; }
    void on_activate() override { advance(); }
    void render(ICanvas& canvas) override;

    // Provide the photo source (e.g. SD card). Resets the slideshow.
    void set_source(IPhotoSource* source) {
        source_ = source;
        index_ = -1;
        decoded_ = false;
        JpegDecoder::free_image(img_);
    }

private:
    void advance();
    void decode_current();
    unsigned photo_count() const { return source_ ? source_->count() : 0; }

    IPhotoSource* source_ = nullptr;
    int index_ = -1;          // current photo; -1 before first advance
    bool decoded_ = false;    // is img_ valid for the current index?
    DecodedImage img_{};
};

}  // namespace lf
