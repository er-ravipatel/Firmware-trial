// ClockPlugin — the first non-photo screen (local, no network needed). Shows time since
// boot as MM:SS. Once we have an RTC/NTP source this becomes a real wall clock.
#pragma once
#include <stdint.h>
#include "../display/ScreenPlugin.h"

namespace lf {

class ClockPlugin : public ScreenPlugin {
public:
    explicit ClockPlugin(const uint32_t* elapsed_ms) : ms_(elapsed_ms) {}

    const char* id() const override { return "clock"; }
    bool wants_continuous_redraw() const override { return true; }
    void render(ICanvas& canvas) override;

private:
    const uint32_t* ms_;
};

}  // namespace lf
