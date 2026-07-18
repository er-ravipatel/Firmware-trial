#include "PhotoFramePlugin.h"
#include "../display/ICanvas.h"
#include "../content/test_image.h"   // embedded fallback JPEG
#include <stdlib.h>

namespace lf {

// Working resolution: decoded photos are downscaled to fit within this (long side) so per-frame
// sampling is fast and cache-friendly while staying sharp at full-screen (1366px) with zoom.
static const unsigned kWorkMax = 1600;

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Area-average downscale of `src` into the fixed buffer `dst.rgb` (capacity kWorkMax^2*3),
// preserving aspect within kWorkMax. If src already fits, it is copied. Sets dst.w/dst.h.
static void downscale_into(const DecodedImage& src, DecodedImage& dst) {
    if (src.rgb == nullptr || src.w == 0 || src.h == 0) {
        dst.w = dst.h = 0;
        return;
    }
    unsigned nw = src.w, nh = src.h;
    if (nw > kWorkMax || nh > kWorkMax) {
        if (src.w >= src.h) {
            nw = kWorkMax;
            nh = (unsigned) ((unsigned long) src.h * kWorkMax / src.w);
        } else {
            nh = kWorkMax;
            nw = (unsigned) ((unsigned long) src.w * kWorkMax / src.h);
        }
        if (nw == 0) nw = 1;
        if (nh == 0) nh = 1;
    }
    uint8_t* d0 = dst.rgb;
    if (nw == src.w && nh == src.h) {
        for (unsigned long i = 0; i < (unsigned long) nw * nh * 3; ++i) d0[i] = src.rgb[i];
    } else {
        for (unsigned dy = 0; dy < nh; ++dy) {
            unsigned sy0 = (unsigned) ((unsigned long) dy * src.h / nh);
            unsigned sy1 = (unsigned) ((unsigned long) (dy + 1) * src.h / nh);
            if (sy1 <= sy0) sy1 = sy0 + 1;
            for (unsigned dx = 0; dx < nw; ++dx) {
                unsigned sx0 = (unsigned) ((unsigned long) dx * src.w / nw);
                unsigned sx1 = (unsigned) ((unsigned long) (dx + 1) * src.w / nw);
                if (sx1 <= sx0) sx1 = sx0 + 1;
                unsigned r = 0, g = 0, b = 0, cnt = 0;
                for (unsigned sy = sy0; sy < sy1; ++sy) {
                    const uint8_t* p = src.rgb + ((unsigned long) sy * src.w + sx0) * 3;
                    for (unsigned sx = sx0; sx < sx1; ++sx, p += 3) {
                        r += p[0]; g += p[1]; b += p[2]; ++cnt;
                    }
                }
                uint8_t* d = d0 + ((unsigned long) dy * nw + dx) * 3;
                d[0] = (uint8_t) (r / cnt);
                d[1] = (uint8_t) (g / cnt);
                d[2] = (uint8_t) (b / cnt);
            }
        }
    }
    dst.w = nw;
    dst.h = nh;
}

void PhotoFramePlugin::reset() {
    index_ = -1;
    next_index_ = -1;
    preloaded_ = false;
    state_ = State::Empty;
    cur_.w = cur_.h = 0;
    next_.w = next_.h = 0;
}

void PhotoFramePlugin::ensure_buffers(unsigned fw, unsigned fh) {
    if (workA_ == nullptr) {
        unsigned long work = (unsigned long) kWorkMax * kWorkMax * 3;
        workA_ = (uint8_t*) malloc(work);
        workB_ = (uint8_t*) malloc(work);
        cur_.rgb = workA_;
        next_.rgb = workB_;
    }
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

void PhotoFramePlugin::load(int idx, DecodedImage& dst) {
    const uint8_t* data = nullptr;
    unsigned len = 0;
    if (photo_count() > 0 && idx >= 0) {
        data = source_->jpeg(unsigned(idx), len);
    }
    if (data == nullptr) {
        data = lumen_test_jpg;
        len = lumen_test_jpg_len;
    }

    unsigned t0 = us();
    DecodedImage tmp{};                     // pool-backed, transient (valid until next decode)
    bool ok = JpegDecoder::decode(data, len, tmp);
    unsigned t1 = us();
    unsigned ow = ok ? tmp.w : 0, oh = ok ? tmp.h : 0;
    if (ok) {
        downscale_into(tmp, dst);           // copy/shrink into dst's fixed work buffer
    } else {
        dst.w = dst.h = 0;
    }
    unsigned t2 = us();

    stats_.index = idx;
    stats_.jpeg_bytes = len;
    stats_.orig_w = ow;
    stats_.orig_h = oh;
    stats_.work_w = dst.w;
    stats_.work_h = dst.h;
    stats_.decode_ms = (t1 - t0) / 1000;
    stats_.scale_ms = (t2 - t1) / 1000;
    stats_pending_ = true;
}

// Ken Burns: animate zoom + pan over `progress` [0,1], sampled nearest-neighbor (fast; the work
// image is already area-averaged, so quality holds).
void PhotoFramePlugin::render_kb(const DecodedImage& img, float progress, int variant,
                                 uint8_t* dst) {
    const unsigned W = fw_, H = fh_;
    if (img.rgb == nullptr || img.w == 0 || img.h == 0) {
        for (unsigned long i = 0; i < (unsigned long) W * H * 3; ++i) dst[i] = 0;
        return;
    }
    progress = clampf(progress, 0.0f, 1.0f);

    float z0, z1, pxs, pys;
    switch (variant & 3) {
        default:
        case 0: z0 = 1.00f; z1 = 1.14f; pxs =  0.04f; pys =  0.03f; break;
        case 1: z0 = 1.14f; z1 = 1.00f; pxs = -0.04f; pys = -0.03f; break;
        case 2: z0 = 1.00f; z1 = 1.12f; pxs = -0.04f; pys =  0.03f; break;
        case 3: z0 = 1.12f; z1 = 1.00f; pxs =  0.04f; pys = -0.03f; break;
    }
    float z = lerpf(z0, z1, progress);
    float panX = lerpf(-pxs, pxs, progress) * (float) img.w;
    float panY = lerpf(-pys, pys, progress) * (float) img.h;

    // COVER: fill the whole frame (crop overflow).
    float coverScale = (float) W / img.w;
    float s2 = (float) H / img.h;
    if (s2 > coverScale) coverScale = s2;
    float invS = 1.0f / (coverScale * z);
    float srcCx = img.w * 0.5f + panX;
    float srcCy = img.h * 0.5f + panY;

    const long kOne = 65536;
    long invS_fp = (long) (invS * kOne);
    long fxBase = (long) (((-(float) W * 0.5f) * invS + srcCx) * kOne);
    long fyBase = (long) (((-(float) H * 0.5f) * invS + srcCy) * kOne);

    const int iw = (int) img.w, ih = (int) img.h;
    const uint8_t* src = img.rgb;

    long fy = fyBase;
    for (unsigned oy = 0; oy < H; ++oy, fy += invS_fp) {
        int y0 = (int) (fy >> 16);
        uint8_t* drow = dst + (unsigned long) oy * W * 3;
        long fx = fxBase;
        for (unsigned ox = 0; ox < W; ++ox, fx += invS_fp, drow += 3) {
            int x0 = (int) (fx >> 16);
            if (x0 < 0 || y0 < 0 || x0 >= iw || y0 >= ih) {
                drow[0] = drow[1] = drow[2] = 0;
            } else {
                const uint8_t* p = src + ((unsigned long) y0 * iw + x0) * 3;
                drow[0] = p[0];
                drow[1] = p[1];
                drow[2] = p[2];
            }
        }
    }
}

void PhotoFramePlugin::render(ICanvas& canvas) {
    const unsigned W = canvas.width();
    const unsigned H = canvas.height();
    ensure_buffers(W, H);

    unsigned n = photo_count();
    unsigned t = now();

    if (state_ == State::Empty) {
        index_ = (n > 0) ? 0 : -1;
        cur_variant_ = variant_of(index_ < 0 ? 0 : index_);
        load(index_, cur_);
        state_ = State::Showing;
        photo_start_ = t;
        preloaded_ = false;
    } else if (state_ == State::Showing) {
        if (n > 1 && !preloaded_ && t - photo_start_ >= kDwellMs / 3) {
            next_index_ = (index_ + 1) % int(n);
            next_variant_ = variant_of(next_index_);
            load(next_index_, next_);
            preloaded_ = true;
        }
        if (n > 1 && t - photo_start_ >= kDwellMs) {
            if (!preloaded_) {
                next_index_ = (index_ + 1) % int(n);
                next_variant_ = variant_of(next_index_);
                load(next_index_, next_);
            }
            index_ = next_index_;
            state_ = State::Fading;
            fade_start_ = t;
        }
    } else {  // Fading
        if (t - fade_start_ >= kFadeMs) {
            DecodedImage tmp = cur_; cur_ = next_; next_ = tmp;   // swap the two work buffers
            cur_variant_ = next_variant_;
            state_ = State::Showing;
            photo_start_ = fade_start_;
            preloaded_ = false;
        }
    }

    // --- Draw fullscreen ---
    if (state_ == State::Fading) {
        float pOut = (float) (t - photo_start_) / kSpanMs;
        float pIn = (float) (t - fade_start_) / kSpanMs;
        render_kb(cur_, pOut, cur_variant_, fbA_);
        render_kb(next_, pIn, next_variant_, fbB_);
        unsigned alpha = (t - fade_start_) * 256 / kFadeMs;
        if (alpha > 256) alpha = 256;
        canvas.blit_rgb_blend(0, 0, fw_, fh_, fbA_, fbB_, alpha);
    } else {
        float p = (float) (t - photo_start_) / kSpanMs;
        render_kb(cur_, p, cur_variant_, fbA_);
        canvas.blit_rgb(0, 0, fw_, fh_, fbA_);
    }
}

}  // namespace lf
