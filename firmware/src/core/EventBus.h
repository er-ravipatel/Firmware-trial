// EventBus — how tasks talk to the render state machine without shared mutable state.
// Web/USB/scheduler tasks post events; the render task is the sole consumer, keeping
// render state lock-free (IMPLEMENTATION §2.2, §3.3).
#pragma once
#include <stdint.h>
#include "../util/RingBuffer.h"

namespace lf {

enum class EventType : uint16_t {
    None = 0,
    DwellExpire,          // slideshow interval elapsed
    CmdNext,
    CmdPrev,
    CmdPause,
    CmdResume,
    SchedSleep,           // night window start
    SchedWake,            // morning window start
    ImportDone,           // content import finished (arg = count)
    PhotosAdded,          // library became non-empty
    PhotosEmpty,          // library became empty
    Motion,               // PIR wake (optional hardware)
    TransitionComplete,   // compositor finished a transition
};

struct Event {
    EventType type = EventType::None;
    int32_t arg = 0;      // optional payload (e.g. imported count)
};

template <size_t N = 32>
class EventBus {
public:
    // Critical events: returns false if the queue is full (caller decides).
    bool post(const Event& e) { return queue_.push(e); }

    // Non-critical events: never fails; drops the oldest if full.
    void post_lossy(const Event& e) { queue_.push_overwrite(e); }

    // Consumer side (render task only).
    bool poll(Event& out) { return queue_.pop(out); }

    size_t pending() const { return queue_.size(); }

private:
    RingBuffer<Event, N> queue_;
};

}  // namespace lf
