#include "PhotoFramePlugin.h"
#include "../display/ICanvas.h"
#include "../content/test_image.h"   // embedded fallback JPEG
#include <stdlib.h>
#include <qrcodegen.h>   // on-device QR (freestanding C); via EXTRAINCLUDE=-I../vendor/qrcodegen

namespace lf {

// Working resolution: decoded photos are downscaled to fit within this (long side).
static const unsigned kWorkMax = 1600;

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Area-average downscale of `src` into the fixed buffer `dst.rgb` (capacity kWorkMax^2*3).
static void downscale_into(const DecodedImage& src, DecodedImage& dst) {
    if (src.rgb == nullptr || src.w == 0 || src.h == 0) {
        dst.w = dst.h = 0;
        return;
    }
    unsigned nw = src.w, nh = src.h;
    if (nw > kWorkMax || nh > kWorkMax) {
        if (src.w >= src.h) { nw = kWorkMax; nh = (unsigned)((unsigned long)src.h * kWorkMax / src.w); }
        else                { nh = kWorkMax; nw = (unsigned)((unsigned long)src.w * kWorkMax / src.h); }
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
                    for (unsigned sx = sx0; sx < sx1; ++sx, p += 3) { r += p[0]; g += p[1]; b += p[2]; ++cnt; }
                }
                uint8_t* d = d0 + ((unsigned long) dy * nw + dx) * 3;
                d[0] = (uint8_t) (r / cnt); d[1] = (uint8_t) (g / cnt); d[2] = (uint8_t) (b / cnt);
            }
        }
    }
    dst.w = nw;
    dst.h = nh;
}

void PhotoFramePlugin::reset() {
    bg_drain();                  // stop the worker touching next_ before we reuse the buffers
    index_ = -1;
    next_index_ = -1;
    preloaded_ = false;
    bg_posted_ = false;
    cur_convert_ = false;
    next_convert_ = false;
    state_ = State::Empty;
    cur_.w = cur_.h = 0;
    next_.w = next_.h = 0;
}

// Worker-core service: if core 0 posted a decode job, run it (decode + scale + blur into the
// next_ buffers) entirely off the render path. Returns true if it decoded something this call.
bool PhotoFramePlugin::bg_service() {
    if (!__atomic_load_n(&bg_req_, __ATOMIC_ACQUIRE)) {
        return false;
    }
    __atomic_store_n(&bg_req_, false, __ATOMIC_RELAXED);
    __atomic_store_n(&bg_busy_, true, __ATOMIC_RELAXED);
    load(bg_index_, next_, next_bg_);
    __atomic_store_n(&bg_busy_, false, __ATOMIC_RELAXED);
    __atomic_store_n(&bg_done_, true, __ATOMIC_RELEASE);   // publishes the buffer writes to core 0
    return true;
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
    if (bgA_) free(bgA_);
    if (bgB_) free(bgB_);
    fw_ = fw;
    fh_ = fh;
    unsigned long fbsz = (unsigned long) fw_ * fh_ * 3;
    fbA_ = (uint8_t*) malloc(fbsz);
    fbB_ = (uint8_t*) malloc(fbsz);
    bgA_ = (uint8_t*) malloc(fbsz);
    bgB_ = (uint8_t*) malloc(fbsz);
    cur_bg_ = bgA_;
    next_bg_ = bgB_;
}

// Build a soft, darkened, full-screen blurred background from `img` (one-time per photo):
// area-average to a tiny image, then bilinear-upscale to full screen (the upscale is the blur).
void PhotoFramePlugin::compute_blur(const DecodedImage& img, uint8_t* bg) {
    const unsigned W = fw_, H = fh_;
    if (img.rgb == nullptr || img.w == 0 || img.h == 0) {
        for (unsigned long i = 0; i < (unsigned long) W * H * 3; ++i) bg[i] = 0;
        return;
    }
    const unsigned TW = 48, TH = 27;
    static uint8_t tiny[TW * TH * 3];
    for (unsigned ty = 0; ty < TH; ++ty) {
        unsigned sy0 = (unsigned) ((unsigned long) ty * img.h / TH);
        unsigned sy1 = (unsigned) ((unsigned long) (ty + 1) * img.h / TH);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        for (unsigned tx = 0; tx < TW; ++tx) {
            unsigned sx0 = (unsigned) ((unsigned long) tx * img.w / TW);
            unsigned sx1 = (unsigned) ((unsigned long) (tx + 1) * img.w / TW);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            unsigned r = 0, g = 0, b = 0, cnt = 0;
            for (unsigned sy = sy0; sy < sy1; ++sy) {
                const uint8_t* p = img.rgb + ((unsigned long) sy * img.w + sx0) * 3;
                for (unsigned sx = sx0; sx < sx1; ++sx, p += 3) { r += p[0]; g += p[1]; b += p[2]; ++cnt; }
            }
            uint8_t* d = tiny + (ty * TW + tx) * 3;
            d[0] = (uint8_t) (r / cnt); d[1] = (uint8_t) (g / cnt); d[2] = (uint8_t) (b / cnt);
        }
    }
    // Bilinear upscale tiny -> bg, darkened so the sharp foreground stands out.
    for (unsigned oy = 0; oy < H; ++oy) {
        float fy = (float) oy * (TH - 1) / (H - 1);
        int y0 = (int) fy; if (y0 >= (int) TH - 1) y0 = TH - 2;
        float dy = fy - y0;
        uint8_t* drow = bg + (unsigned long) oy * W * 3;
        for (unsigned ox = 0; ox < W; ++ox) {
            float fx = (float) ox * (TW - 1) / (W - 1);
            int x0 = (int) fx; if (x0 >= (int) TW - 1) x0 = TW - 2;
            float dx = fx - x0;
            const uint8_t* p00 = tiny + (y0 * TW + x0) * 3;
            const uint8_t* p10 = p00 + 3;
            const uint8_t* p01 = p00 + TW * 3;
            const uint8_t* p11 = p01 + 3;
            float w00 = (1 - dx) * (1 - dy), w10 = dx * (1 - dy), w01 = (1 - dx) * dy, w11 = dx * dy;
            for (int c = 0; c < 3; ++c) {
                float v = p00[c] * w00 + p10[c] * w10 + p01[c] * w01 + p11[c] * w11;
                drow[ox * 3 + c] = (uint8_t) (v * 0.55f);   // darken
            }
        }
    }
}

void PhotoFramePlugin::load(int idx, DecodedImage& dst, uint8_t* bg) {
    const uint8_t* data = nullptr;
    unsigned len = 0;
    if (photo_count() > 0 && idx >= 0) data = source_->jpeg(unsigned(idx), len);
    if (data == nullptr) { data = lumen_test_jpg; len = lumen_test_jpg_len; }

    unsigned t0 = us();
    DecodedImage tmp{};
    bool ok = JpegDecoder::decode(data, len, tmp);
    unsigned t1 = us();
    unsigned ow = ok ? tmp.w : 0, oh = ok ? tmp.h : 0;
    if (ok) downscale_into(tmp, dst); else dst.w = dst.h = 0;
    compute_blur(dst, bg);
    unsigned t2 = us();

    stats_.index = idx;
    stats_.jpeg_bytes = len;
    stats_.orig_w = ow; stats_.orig_h = oh;
    stats_.work_w = dst.w; stats_.work_h = dst.h;
    stats_.decode_ms = (t1 - t0) / 1000;
    stats_.scale_ms = (t2 - t1) / 1000;
    stats_pending_ = true;
}

// Composite: blurred background everywhere, with the FIT photo (whole image visible) drawn on
// top via a gentle Ken Burns. Nothing important is cropped; empty space shows the blur.
void PhotoFramePlugin::render_photo(const DecodedImage& img, const uint8_t* bg, float progress,
                                    int variant, uint8_t* dst) {
    const unsigned W = fw_, H = fh_;
    if (img.rgb == nullptr || img.w == 0 || img.h == 0) {
        for (unsigned long i = 0; i < (unsigned long) W * H * 3; ++i) dst[i] = bg ? bg[i] : 0;
        return;
    }
    progress = clampf(progress, 0.0f, 1.0f);

    float z0, z1, pxs, pys;
    switch (variant & 3) {
        default:
        case 0: z0 = 1.00f; z1 = 1.06f; pxs =  0.010f; pys =  0.008f; break;
        case 1: z0 = 1.06f; z1 = 1.00f; pxs = -0.010f; pys = -0.008f; break;
        case 2: z0 = 1.00f; z1 = 1.05f; pxs = -0.010f; pys =  0.008f; break;
        case 3: z0 = 1.05f; z1 = 1.00f; pxs =  0.010f; pys = -0.008f; break;
    }
    float z = lerpf(z0, z1, progress);
    float panX = lerpf(-pxs, pxs, progress) * (float) img.w;
    float panY = lerpf(-pys, pys, progress) * (float) img.h;

    // FIT: whole image visible (letterboxed) at z=1; gentle zoom crops only a few % at edges.
    float fitScale = (float) W / img.w;
    float s2 = (float) H / img.h;
    if (s2 < fitScale) fitScale = s2;
    float invS = 1.0f / (fitScale * z);
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
        const uint8_t* bgrow = bg + (unsigned long) oy * W * 3;
        long fx = fxBase;
        for (unsigned ox = 0; ox < W; ++ox, fx += invS_fp, drow += 3) {
            int x0 = (int) (fx >> 16);
            if (x0 >= 0 && y0 >= 0 && x0 < iw && y0 < ih) {
                const uint8_t* p = src + ((unsigned long) y0 * iw + x0) * 3;
                drow[0] = p[0]; drow[1] = p[1]; drow[2] = p[2];
            } else {
                drow[0] = bgrow[ox * 3];       // outside the photo -> blurred background
                drow[1] = bgrow[ox * 3 + 1];
                drow[2] = bgrow[ox * 3 + 2];
            }
        }
    }
}

void PhotoFramePlugin::intro_load() {
    // Decode photo 0 into cur_/cur_bg_ and render the first sharp frame into fbA_, so the kernel
    // can build the photo-hero splash. Leaves the plugin ready to continue from photo 0 with no
    // re-decode (state Showing); the dwell timer starts when on_activate() runs after the intro.
    unsigned n = photo_count();
    index_ = (n > 0) ? 0 : -1;
    cur_variant_ = variant_of(index_ < 0 ? 0 : index_);
    cur_convert_ = index_needs_convert(index_);
    if (!cur_convert_) {
        load(index_, cur_, cur_bg_);
        render_photo(cur_, cur_bg_, 0.0f, cur_variant_, fbA_);
    } else {
        // First file is a needs-convert placeholder: dissolve into black, then the loop shows the QR.
        cur_.w = cur_.h = 0;
        for (unsigned long i = 0; i < (unsigned long) fw_ * fh_ * 3; ++i) fbA_[i] = 0;
    }
    state_ = State::Showing;
    preloaded_ = false;
    bg_posted_ = false;
}

void PhotoFramePlugin::set_convert_qr(const char* payload) {
    unsigned i = 0;
    for (; payload && payload[i] && i + 1 < sizeof(qr_payload_); ++i) qr_payload_[i] = payload[i];
    qr_payload_[i] = '\0';
}

void PhotoFramePlugin::set_convert_hint(const char* ssid) {
    unsigned i = 0;
    for (; ssid && ssid[i] && i + 1 < sizeof(convert_hint_); ++i) convert_hint_[i] = ssid[i];
    convert_hint_[i] = '\0';
}

// Encode `text` as a QR and draw it centered at (cx,cy) via the device-neutral canvas.
void PhotoFramePlugin::draw_qr(ICanvas& canvas, const char* text, unsigned cx, unsigned cy, unsigned mod) {
    static const int kMaxVer = 8;
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(kMaxVer)];
    uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(kMaxVer)];
    if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, kMaxVer, qrcodegen_Mask_AUTO, true)) {
        return;
    }
    int qn = qrcodegen_getSize(qr);
    const unsigned quiet = 4;
    unsigned dim = (unsigned)(qn + 2 * (int)quiet) * mod;
    unsigned x0 = cx - dim / 2, y0 = cy - dim / 2;
    canvas.fill_rect(x0, y0, dim, dim, Rgb{255, 255, 255});
    for (int my = 0; my < qn; ++my)
        for (int mx = 0; mx < qn; ++mx)
            if (qrcodegen_getModule(qr, mx, my))
                canvas.fill_rect(x0 + (quiet + (unsigned)mx) * mod,
                                 y0 + (quiet + (unsigned)my) * mod, mod, mod, Rgb{0, 0, 0});
}

// "Needs-convert" placeholder: dark background + a single, progressive QR guide. Step 1 shows a
// Wi-Fi-join QR; once a phone has joined the AP (net_ready_) the slide switches live to step 2, a
// URL QR that opens the converter in the phone's browser. One QR at a time keeps it unambiguous.
// Non-disruptive — the slideshow moves on after the dwell.
void PhotoFramePlugin::render_convert_slide(ICanvas& canvas, const char* filename) {
    const unsigned W = canvas.width(), H = canvas.height();
    canvas.clear(Rgb{22, 16, 34});   // dark plum, matching the brand

    const Rgb accent{232, 196, 138}, dim{198, 208, 222}, faint{150, 132, 158}, ok{140, 214, 170};
    auto ctext = [&](const char* s, unsigned y, Rgb c) {   // centered horizontally
        unsigned n = 0; while (s[n]) n++;
        canvas.text(W / 2 - n * 4, y, s, c);
    };

    unsigned mod = H / 150; if (mod < 4) mod = 4; if (mod > 7) mod = 7;
    unsigned qdim = 37 * mod;
    unsigned qcy = H / 2 + 6;

    ctext("THIS PHOTO NEEDS CONVERTING", qcy - qdim / 2 - 30, accent);

    bool connected = net_ready_ && *net_ready_;
    unsigned y = qcy + qdim / 2 + 16;

    if (!connected && convert_hint_[0]) {
        // Step 1 — join the frame's Wi-Fi (QR encodes an open-network join).
        char wifi[80]; unsigned k = 0;
        for (const char* p = "WIFI:S:"; *p && k + 1 < sizeof wifi; ++p) wifi[k++] = *p;
        for (const char* p = convert_hint_; *p && k + 1 < sizeof wifi; ++p) wifi[k++] = *p;
        for (const char* p = ";T:nopass;;"; *p && k + 1 < sizeof wifi; ++p) wifi[k++] = *p;
        wifi[k] = '\0';
        draw_qr(canvas, wifi, W / 2, qcy, mod);
        ctext("Step 1 of 2  -  Scan to join Wi-Fi", y, dim);   y += 22;
        ctext(convert_hint_, y, accent);
    } else {
        // Step 2 — phone is on the AP (or no SSID to show): scan to open the converter.
        draw_qr(canvas, qr_payload_, W / 2, qcy, mod);
        if (connected) { ctext("Connected  -  Step 2 of 2", y, ok); y += 22; }
        ctext("Scan to open the converter", y, dim);
    }

    if (filename && filename[0]) ctext(filename, qcy + qdim / 2 + 66, faint);
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
        cur_convert_ = index_needs_convert(index_);
        if (!cur_convert_) load(index_, cur_, cur_bg_);   // decode only displayable files
        state_ = State::Showing;
        photo_start_ = t;
        preloaded_ = false;
        bg_posted_ = false;
    } else if (state_ == State::Showing) {
        // Kick off the next slide a third of the way through the dwell. A needs-convert slide has
        // nothing to decode — it's a QR placeholder — so mark it ready immediately.
        if (n > 1 && !bg_posted_ && !preloaded_ && t - photo_start_ >= kDwellMs / 3) {
            next_index_ = (index_ + 1) % int(n);
            next_variant_ = variant_of(next_index_);
            next_convert_ = index_needs_convert(next_index_);
            if (next_convert_) { preloaded_ = true; bg_posted_ = true; }
            else { bg_request(next_index_); bg_posted_ = true; }
        }
        // Pick up the finished decode.
        if (bg_posted_ && !preloaded_ && bg_poll_done()) {
            preloaded_ = true;
        }
        // Safety net: if the worker never ran (multicore unavailable) well past the dwell, decode
        // inline so the slideshow still advances. Only when the worker is provably idle (no race).
        if (n > 1 && !next_convert_ && bg_posted_ && !preloaded_
            && t - photo_start_ >= kDwellMs + 2500 && !bg_in_flight()) {
            load(next_index_, next_, next_bg_);
            preloaded_ = true;
        }
        // Advance once the next slide is ready AND the dwell elapsed. Cross-fade between photos;
        // hard-cut when a QR/convert slide is involved (there is no image to blend).
        if (n > 1 && preloaded_ && t - photo_start_ >= kDwellMs) {
            index_ = next_index_;
            if (cur_convert_ || next_convert_) {
                DecodedImage tmpi = cur_; cur_ = next_; next_ = tmpi;
                uint8_t* tmpb = cur_bg_; cur_bg_ = next_bg_; next_bg_ = tmpb;
                cur_variant_ = next_variant_;
                cur_convert_ = next_convert_;
                state_ = State::Showing;
                photo_start_ = t;
                preloaded_ = false;
                bg_posted_ = false;
            } else {
                state_ = State::Fading;
                fade_start_ = t;
            }
        }
    } else {  // Fading (photos only — convert slides hard-cut)
        if (t - fade_start_ >= kFadeMs) {
            DecodedImage tmpi = cur_; cur_ = next_; next_ = tmpi;
            uint8_t* tmpb = cur_bg_; cur_bg_ = next_bg_; next_bg_ = tmpb;
            cur_variant_ = next_variant_;
            cur_convert_ = next_convert_;
            state_ = State::Showing;
            photo_start_ = fade_start_;
            preloaded_ = false;
            bg_posted_ = false;
        }
    }

    // --- Draw fullscreen ---
    if (cur_convert_ && state_ != State::Fading) {
        render_convert_slide(canvas, index_name(index_));
    } else if (state_ == State::Fading) {
        float pOut = (float) (t - photo_start_) / kSpanMs;
        float pIn = (float) (t - fade_start_) / kSpanMs;
        render_photo(cur_, cur_bg_, pOut, cur_variant_, fbA_);
        render_photo(next_, next_bg_, pIn, next_variant_, fbB_);
        unsigned alpha = (t - fade_start_) * 256 / kFadeMs;
        if (alpha > 256) alpha = 256;
        canvas.blit_rgb_blend(0, 0, fw_, fh_, fbA_, fbB_, alpha);
    } else {
        float p = (float) (t - photo_start_) / kSpanMs;
        render_photo(cur_, cur_bg_, p, cur_variant_, fbA_);
        canvas.blit_rgb(0, 0, fw_, fh_, fbA_);
    }
}

}  // namespace lf
