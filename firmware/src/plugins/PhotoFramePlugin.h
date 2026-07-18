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

    // Wi-Fi-join QR payload drawn on "needs-convert" slides (set once by the kernel from config).
    void set_convert_qr(const char* payload);

    // Wait for the background decoder (core 1) to go idle — call before re-scanning the source
    // from core 0 (e.g. after a conversion writes a new file), so the two don't race on FatFs.
    void drain_decode() { bg_drain(); }

    // Restart the slideshow (e.g. after the photo source changes at runtime).
    void reset();

    // --- Boot-splash support ---
    // The kernel composes a "photo-hero" splash from the first photo's buffers: it shows the
    // blurred first photo behind the wordmark, then cross-fades ("sharpens") into the live frame.
    void intro_alloc(unsigned w, unsigned h) { ensure_buffers(w, h); }  // allocate buffers (fast)
    void intro_load();                    // decode photo 0 -> cur_/cur_bg_, render sharp -> fbA_
    const uint8_t* intro_bg() const { return cur_bg_; }   // blurred+darkened first photo (fullscreen)
    const uint8_t* intro_sharp() const { return fbA_; }   // sharp first slideshow frame (fullscreen)
    uint8_t* intro_scratch() { return fbB_; }             // spare fullscreen buffer (kernel scratch)
    unsigned fb_w() const { return fw_; }
    unsigned fb_h() const { return fh_; }

    // --- Background decode (runs on a second CPU core) ---
    // The render loop (core 0) never decodes inline; it posts a job and keeps rendering while a
    // worker core decodes+scales+blurs the next photo. Handshake uses GCC atomic builtins
    // (acquire/release) so it stays freestanding — no Circle headers — while emitting the right
    // memory barriers. cur_/cur_bg_ are read by core 0; next_/next_bg_ are written by the worker;
    // they are disjoint during a job, and the pointer swap happens only on core 0 when idle.
    bool bg_service();   // called repeatedly by the worker core; returns true if it did work

    // --- Perf instrumentation (read by the kernel for logging) ---
    struct LoadStats {
        int index = -1;
        unsigned jpeg_bytes = 0;
        unsigned orig_w = 0, orig_h = 0, work_w = 0, work_h = 0;
        unsigned decode_ms = 0, scale_ms = 0;
    };
    void set_time_us(unsigned (*fn)()) { time_us_ = fn; }   // microsecond clock for measuring
    bool take_load_stats(LoadStats& out) {                  // true (and clears) if new stats
        if (!stats_pending_) return false;
        out = stats_;
        stats_pending_ = false;
        return true;
    }
    bool is_transitioning() const { return state_ == State::Fading; }

private:
    enum class State { Empty, Showing, Fading };

    void ensure_buffers(unsigned fw, unsigned fh);
    void load(int idx, DecodedImage& img, uint8_t* bg);
    void compute_blur(const DecodedImage& img, uint8_t* bg);
    // Composite one frame: blurred background + fit photo (Ken Burns) at progress [0,1].
    void render_photo(const DecodedImage& img, const uint8_t* bg, float progress, int variant,
                      uint8_t* dst);
    // "Needs-convert" placeholder slide: dark background + Wi-Fi-join QR + "scan to convert" text.
    void render_convert_slide(ICanvas& canvas, const char* filename);
    void draw_qr(ICanvas& canvas, const char* text, unsigned cx, unsigned cy, unsigned mod);
    bool index_needs_convert(int idx) const { return source_ && idx >= 0 && source_->needs_convert(unsigned(idx)); }
    const char* index_name(int idx) const { return (source_ && idx >= 0) ? source_->name(unsigned(idx)) : ""; }

    unsigned now() const { return ms_ ? *ms_ : 0; }
    unsigned photo_count() const { return source_ ? source_->count() : 0; }
    static int variant_of(int idx) { return idx & 3; }

    // --- Background-decode handshake (core 0 <-> worker core) ---
    void bg_request(int idx) {                    // core 0: post a decode job for the worker
        bg_index_ = idx;
        __atomic_store_n(&bg_req_, true, __ATOMIC_RELEASE);
    }
    bool bg_poll_done() {                          // core 0: true once when the job has finished
        if (__atomic_load_n(&bg_done_, __ATOMIC_ACQUIRE)) {
            __atomic_store_n(&bg_done_, false, __ATOMIC_RELAXED);
            return true;
        }
        return false;
    }
    bool bg_in_flight() const {                    // core 0: worker has an unfinished job
        return __atomic_load_n(&bg_req_, __ATOMIC_ACQUIRE)
            || __atomic_load_n(&bg_busy_, __ATOMIC_ACQUIRE);
    }
    void bg_drain() {                              // core 0: wait for the worker to go idle
        while (bg_in_flight()) { }
        __atomic_store_n(&bg_done_, false, __ATOMIC_RELAXED);
        bg_posted_ = false;
    }

    static const unsigned kDwellMs = 10000;  // display time per photo (longer => slower Ken Burns)
    static const unsigned kFadeMs = 1400;    // cross-fade duration (gentler dissolve)
    static const unsigned kSpanMs = kDwellMs + kFadeMs;  // total on-screen span (KB timeline)

    IPhotoSource* source_ = nullptr;
    const uint32_t* ms_ = nullptr;

    int index_ = -1;
    int next_index_ = -1;
    bool preloaded_ = false;     // has the next photo been decoded ahead of time?
    bool bg_posted_ = false;     // core 0: a background decode job is outstanding
    bool cur_convert_ = false;   // is the current slide a needs-convert placeholder (show QR)?
    bool next_convert_ = false;
    char qr_payload_[80] = {0};  // Wi-Fi-join QR string drawn on convert slides
    int cur_variant_ = 0, next_variant_ = 0;
    State state_ = State::Empty;
    unsigned photo_start_ = 0;   // when the current photo first appeared
    unsigned fade_start_ = 0;    // when the current fade began

    // Cross-core flags (touched via __atomic_* only). bg_req_: core0->worker; bg_busy_: worker
    // is decoding; bg_done_: worker->core0. bg_index_ is set before bg_req_ is released.
    int  bg_index_ = -1;
    bool bg_req_ = false;
    bool bg_busy_ = false;
    bool bg_done_ = false;

    DecodedImage cur_{}, next_{};  // .rgb point at workA_/workB_ (reused, never per-photo malloc)
    uint8_t* fbA_ = nullptr;       // rendered frame for the current/outgoing photo
    uint8_t* fbB_ = nullptr;       // rendered frame for the incoming photo
    uint8_t* workA_ = nullptr;     // fixed decoded-image buffers (kWorkMax^2*3), reused per photo
    uint8_t* workB_ = nullptr;
    uint8_t* bgA_ = nullptr;       // fullscreen blurred backgrounds (fw_*fh_*3), one per work buf
    uint8_t* bgB_ = nullptr;
    uint8_t* cur_bg_ = nullptr;    // background paired with cur_ / next_
    uint8_t* next_bg_ = nullptr;
    unsigned fw_ = 0, fh_ = 0;

    unsigned (*time_us_)() = nullptr;
    unsigned us() const { return time_us_ ? time_us_() : 0; }
    LoadStats stats_{};
    bool stats_pending_ = false;
};

}  // namespace lf
