// PluginScheduler — decides which ScreenPlugin is on-screen (InkyPi-style playlist).
// Pure logic (no heap, no hardware): a fixed set of slots, each with a display duration
// and an optional active time window (minute-of-day, supports midnight wrap). round-robins
// among enabled + currently-active slots. Time is injected (now_min) so it is fully testable
// without a real clock.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../core/Result.h"

namespace lf {

struct PluginSlot {
    const char* id = "";
    bool enabled = true;
    uint16_t duration_s = 30;      // how long to display before advancing
    int16_t window_start_min = -1; // minute-of-day [0..1439]; -1 = always active
    int16_t window_end_min = -1;   // exclusive; if start > end, the window wraps midnight
};

template <size_t MaxSlots = 16>
class PluginScheduler {
public:
    Status add(const PluginSlot& s) {
        if (count_ >= MaxSlots) return Error::Full;
        slots_[count_++] = s;
        return Ok();
    }

    bool set_enabled(const char* id, bool enabled) {
        int i = find(id);
        if (i < 0) return false;
        slots_[size_t(i)].enabled = enabled;
        return true;
    }

    size_t count() const { return count_; }
    const PluginSlot& at(size_t i) const { return slots_[i]; }
    int current() const { return current_; }

    // Advance to the next enabled + active slot given the current minute-of-day.
    // Returns its index, or -1 if no slot is currently showable. If only the current
    // slot is active, returns it again.
    int next(int now_min) {
        if (count_ == 0) return -1;
        int start = current_ + 1;  // current_ == -1 -> start at 0
        for (size_t k = 0; k < count_; ++k) {
            size_t idx = size_t((start + int(k)) % int(count_));
            const PluginSlot& s = slots_[idx];
            if (s.enabled && active_at(s, now_min)) {
                current_ = int(idx);
                return current_;
            }
        }
        return -1;
    }

    static bool active_at(const PluginSlot& s, int now_min) {
        if (s.window_start_min < 0) return true;  // always
        int a = s.window_start_min, b = s.window_end_min;
        if (a <= b) return now_min >= a && now_min < b;
        return now_min >= a || now_min < b;  // wraps midnight
    }

private:
    int find(const char* id) const {
        for (size_t i = 0; i < count_; ++i) {
            if (strcmp(slots_[i].id, id) == 0) return int(i);
        }
        return -1;
    }

    PluginSlot slots_[MaxSlots];
    size_t count_ = 0;
    int current_ = -1;
};

}  // namespace lf
