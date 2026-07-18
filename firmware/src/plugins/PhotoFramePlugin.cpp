#include "PhotoFramePlugin.h"
#include "../display/ICanvas.h"
#include "../content/test_image.h"   // embedded fallback JPEG
#include <stdlib.h>

namespace lf {

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

void PhotoFramePlugin::reset() {
    index_ = -1;
    state_ = State::Empty;
    JpegDecoder::free_image(cur_);
    JpegDecoder::free_image(next_);
}

void PhotoFramePlugin::ensure_buffers(unsigned fw, unsigned fh) {
    if (fbA_ != nullptr && fw == fw_ && fh == fh_) {
        return;
    }
    if (fbA_) free(fbA_);
    if (fbB_) free(fbB_);
    fw_ = fw;
    fh_ = fh;
    fbA_ = (uint8_t*) malloc((unsigned long) fw_ * fh_ * 3);
    fbB_ = (uint8_t*) malloc((unsigned long) fw_ * fh_ * 3);
}

void PhotoFramePlugin::load(int idx, DecodedImage& img) {
    JpegDecoder::free_image(img);
    const uint8_t* data = nullptr;
    unsigned len = 0;
    if (photo_count() > 0 && idx >= 0) {
        data = source_->jpeg(unsigned(idx), len);
    }
    if (data == nullptr) {
        data = lumen_test_jpg;
        len = lumen_test_jpg_len;
    }
    JpegDecoder::decode(data, len, img);
}

// Ken Burns: animate a zoom + pan over `progress` [0,1]. The image is fit into the frame
// (letterboxed); zoom>1 crops in, pan shifts the view. Sampled with bilinear interpolation.
void PhotoFramePlugin::render_kb(const DecodedImage& img, float progress, int variant,
                                 uint8_t* dst) {
    const unsigned W = fw_, H = fh_;
    if (img.rgb == nullptr || img.w == 0 || img.h == 0) {
        for (unsigned i = 0; i < (unsigned long) W * H * 3; ++i) dst[i] = 0;
        return;
    }
    progress = clampf(progress, 0.0f, 1.0f);

    // Per-variant zoom range and pan direction (gentle).
    float z0, z1, pxs, pys;
    switch (variant & 3) {
        default:
        case 0: z0 = 1.00f; z1 = 1.14f; pxs =  0.04f; pys =  0.03f; break;  // zoom in, drift SE
        case 1: z0 = 1.14f; z1 = 1.00f; pxs = -0.04f; pys = -0.03f; break;  // zoom out, drift NW
        case 2: z0 = 1.00f; z1 = 1.12f; pxs = -0.04f; pys =  0.03f; break;  // zoom in, drift SW
        case 3: z0 = 1.12f; z1 = 1.00f; pxs =  0.04f; pys = -0.03f; break;  // zoom out, drift NE
    }
    float z = lerpf(z0, z1, progress);
    float panX = lerpf(-pxs, pxs, progress) * (float) img.w;
    float panY = lerpf(-pys, pys, progress) * (float) img.h;

    // Base fit scale (whole image visible at z=1), then apply zoom.
    float fitScale = (float) W / img.w;
    float s2 = (float) H / img.h;
    if (s2 < fitScale) fitScale = s2;
    float scale = fitScale * z;
    float invS = 1.0f / scale;

    float srcCx = img.w * 0.5f + panX;
    float srcCy = img.h * 0.5f + panY;
    float halfW = W * 0.5f;
    float halfH = H * 0.5f;

    const int iw = (int) img.w, ih = (int) img.h;
    const uint8_t* src = img.rgb;

    for (unsigned oy = 0; oy < H; ++oy) {
        float fy = (oy - halfH) * invS + srcCy;
        uint8_t* drow = dst + (unsigned long) oy * W * 3;
        float fx = (0 - halfW) * invS + srcCx;
        for (unsigned ox = 0; ox < W; ++ox, fx += invS) {
            int x0 = (int) fx;
            int y0 = (int) fy;
            if (x0 < 0 || y0 < 0 || x0 + 1 >= iw || y0 + 1 >= ih) {
                drow[0] = drow[1] = drow[2] = 0;   // outside image -> black (letterbox)
            } else {
                float dx = fx - x0, dy = fy - y0;
                const uint8_t* p00 = src + ((unsigned long) y0 * iw + x0) * 3;
                const uint8_t* p10 = p00 + 3;
                const uint8_t* p01 = p00 + (unsigned long) iw * 3;
                const uint8_t* p11 = p01 + 3;
                float w00 = (1 - dx) * (1 - dy), w10 = dx * (1 - dy);
                float w01 = (1 - dx) * dy, w11 = dx * dy;
                drow[0] = (uint8_t) (p00[0] * w00 + p10[0] * w10 + p01[0] * w01 + p11[0] * w11);
                drow[1] = (uint8_t) (p00[1] * w00 + p10[1] * w10 + p01[1] * w01 + p11[1] * w11);
                drow[2] = (uint8_t) (p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11);
            }
            drow += 3;
        }
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

    unsigned n = photo_count();
    unsigned t = now();

    if (state_ == State::Empty) {
        index_ = (n > 0) ? 0 : -1;
        cur_variant_ = variant_of(index_ < 0 ? 0 : index_);
        load(index_, cur_);
        state_ = State::Showing;
        photo_start_ = t;
    } else if (state_ == State::Showing) {
        if (n > 1 && t - photo_start_ >= kDwellMs) {
            index_ = (index_ + 1) % int(n);
            next_variant_ = variant_of(index_);
            load(index_, next_);
            state_ = State::Fading;
            fade_start_ = t;
        }
    } else {  // Fading
        if (t - fade_start_ >= kFadeMs) {
            // adopt the incoming photo
            DecodedImage tmp = cur_; cur_ = next_; next_ = tmp;
            JpegDecoder::free_image(next_);
            cur_variant_ = next_variant_;
            state_ = State::Showing;
            photo_start_ = fade_start_;   // keep KB timeline continuous
        }
    }

    // --- Draw ---
    canvas.clear(rgb::DarkGray);
    canvas.fill_rect(fx - 6, fy - 6, fw + 12, fh + 12, rgb::White);

    if (state_ == State::Fading) {
        float pOut = (float) (t - photo_start_) / kSpanMs;
        float pIn = (float) (t - fade_start_) / kSpanMs;
        render_kb(cur_, pOut, cur_variant_, fbA_);
        render_kb(next_, pIn, next_variant_, fbB_);
        unsigned alpha = (t - fade_start_) * 255 / kFadeMs;
        if (alpha > 255) alpha = 255;
        canvas.blit_rgb_blend(fx, fy, fw_, fh_, fbA_, fbB_, alpha);
    } else {
        float p = (float) (t - photo_start_) / kSpanMs;
        render_kb(cur_, p, cur_variant_, fbA_);
        canvas.blit_rgb(fx, fy, fw_, fh_, fbA_);
    }

    // Caption.
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
    } else {
        const char* s = "embedded";
        for (const char* p = s; *p; ++p) caption[k++] = *p;
    }
    caption[k] = '\0';
    canvas.text(40, H - 28, caption, rgb::White);
}

}  // namespace lf
