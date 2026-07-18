#include "PhotoFramePlugin.h"
#include "../display/ICanvas.h"
#include "../content/JpegDecoder.h"
#include "../content/test_image.h"   // embedded fallback JPEG
#include <stdlib.h>
#include <string.h>

namespace lf {

void PhotoFramePlugin::ensure_buffers(unsigned fw, unsigned fh) {
    if (cur_ != nullptr && fw == fw_ && fh == fh_) {
        return;
    }
    if (cur_) { free(cur_); cur_ = nullptr; }
    if (next_) { free(next_); next_ = nullptr; }
    fw_ = fw;
    fh_ = fh;
    cur_ = (uint8_t*) malloc((unsigned long) fw_ * fh_ * 3);
    next_ = (uint8_t*) malloc((unsigned long) fw_ * fh_ * 3);
}

// Decode photo `photo_index` (or the embedded fallback) and scale-to-fit into dst (fw_ x fh_),
// preserving aspect ratio with black letterbox bars.
void PhotoFramePlugin::prepare(int photo_index, uint8_t* dst) {
    if (dst == nullptr) {
        return;
    }
    memset(dst, 0, (unsigned long) fw_ * fh_ * 3);   // black background (letterbox)

    const uint8_t* data = nullptr;
    unsigned len = 0;
    if (photo_count() > 0 && photo_index >= 0) {
        data = source_->jpeg(unsigned(photo_index), len);
    }
    if (data == nullptr) {
        data = lumen_test_jpg;
        len = lumen_test_jpg_len;
    }

    DecodedImage img{};
    if (!JpegDecoder::decode(data, len, img) || img.rgb == nullptr) {
        return;   // leaves a black frame
    }

    // Fit img into fw_ x fh_ preserving aspect ratio.
    unsigned dw = fw_, dh = img.h * fw_ / img.w;
    if (dh > fh_) { dh = fh_; dw = img.w * fh_ / img.h; }
    unsigned ox = (fw_ - dw) / 2;
    unsigned oy = (fh_ - dh) / 2;

    for (unsigned r = 0; r < dh; ++r) {
        unsigned sy = r * img.h / dh;
        for (unsigned c = 0; c < dw; ++c) {
            unsigned sx = c * img.w / dw;
            const uint8_t* s = img.rgb + ((unsigned long) sy * img.w + sx) * 3;
            uint8_t* d = dst + ((unsigned long) (oy + r) * fw_ + (ox + c)) * 3;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        }
    }
    JpegDecoder::free_image(img);
}

void PhotoFramePlugin::on_activate() {
    // Reset the dwell so re-entering the plugin shows the current photo before advancing.
    phase_start_ = now();
    if (state_ == State::Fading) {
        state_ = State::Showing;   // don't resume a fade mid-way after being away
    }
}

void PhotoFramePlugin::draw_frame(ICanvas& canvas, unsigned fx, unsigned fy) {
    // Called after clear + mat are drawn. Blits the prepared frame(s).
    if (state_ == State::Fading && cur_ && next_) {
        unsigned t = now() - phase_start_;
        unsigned alpha = t >= kFadeMs ? 255 : (t * 255 / kFadeMs);
        canvas.blit_rgb_blend(fx, fy, fw_, fh_, cur_, next_, alpha);
    } else if (cur_) {
        canvas.blit_rgb(fx, fy, fw_, fh_, cur_);
    }
}

void PhotoFramePlugin::render(ICanvas& canvas) {
    const unsigned W = canvas.width();
    const unsigned H = canvas.height();
    const unsigned fw = W * 3 / 4;
    const unsigned fh = H * 3 / 5;
    const unsigned fx = (W - fw) / 2;
    const unsigned fy = (H - fh) / 2 - 8;

    ensure_buffers(fw, fh);

    // --- Slideshow state machine ---
    unsigned n = photo_count();
    if (state_ == State::Empty) {
        index_ = (n > 0) ? 0 : -1;
        prepare(index_, cur_);
        state_ = State::Showing;
        phase_start_ = now();
    } else if (state_ == State::Showing) {
        if (n > 1 && now() - phase_start_ >= kDwellMs) {
            index_ = (index_ + 1) % int(n);
            prepare(index_, next_);
            state_ = State::Fading;
            phase_start_ = now();
        }
    } else {  // Fading
        if (now() - phase_start_ >= kFadeMs) {
            uint8_t* tmp = cur_; cur_ = next_; next_ = tmp;   // swap in the new photo
            state_ = State::Showing;
            phase_start_ = now();
        }
    }

    // --- Draw ---
    canvas.clear(rgb::DarkGray);
    canvas.fill_rect(fx - 6, fy - 6, fw + 12, fh + 12, rgb::White);
    canvas.fill_rect(fx, fy, fw, fh, rgb::Black);
    draw_frame(canvas, fx, fy);

    canvas.text(40, 12, "PHOTOS", rgb::White);
    char caption[64];
    unsigned k = 0;
    if (n > 0) {
        const char* pfx = "Photo ";
        for (const char* p = pfx; *p; ++p) caption[k++] = *p;
        int shown = (index_ < 0 ? 0 : index_) + 1;
        caption[k++] = char('0' + (shown / 10) % 10);
        caption[k++] = char('0' + shown % 10);
        caption[k++] = '/';
        caption[k++] = char('0' + (n / 10) % 10);
        caption[k++] = char('0' + n % 10);
        const char* sfx = "   cross-fade slideshow (SD)";
        for (const char* p = sfx; *p; ++p) caption[k++] = *p;
    } else {
        const char* s = "embedded fallback";
        for (const char* p = s; *p; ++p) caption[k++] = *p;
    }
    caption[k] = '\0';
    canvas.text(40, H - 28, caption, rgb::White);
}

}  // namespace lf
