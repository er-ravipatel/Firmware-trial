// NecDecoder — freestanding NEC infrared protocol decoder (pulse widths in, key codes out).
//
// Feed it the width of every mark (IR carrier present, receiver output LOW) and space (no
// carrier, output HIGH) in microseconds, in the order they arrive; it emits a validated
// (address, command) pair per key press, and "repeat" events while the key is held.
//
// NEC frame (all times in µs, tolerance ±25 %):
//   lead mark 9000 · lead space 4500 · 32 bits, each = mark 562 + space 562 (0) or 1687 (1),
//   LSB first: address, ~address, command, ~command · trailing mark 562.
//   Repeat frame (key held, every ~108 ms): mark 9000 · space 2250 · mark 562.
//
// Freestanding on purpose (C headers, no std::, no allocation) so the exact same code runs in
// the host unit tests and in the bare-metal firmware. The ISR never calls this; it only records
// pulse widths into a ring which the main loop drains into feed().
#pragma once
#include <stdint.h>

namespace lf {

struct NecKey {
    uint8_t address = 0;
    uint8_t command = 0;
    bool    repeat = false;    // true = the previous key is still held (NEC repeat frame)
    bool    extended = false;  // address bytes were not complementary (NEC "extended" 16-bit addr)
};

class NecDecoder {
public:
    // Timing constants (µs) and tolerance. Public so tests and the IR glue can reuse them.
    static const unsigned kLeadMark   = 9000;
    static const unsigned kLeadSpace  = 4500;
    static const unsigned kRepeatSpace = 2250;
    static const unsigned kBitMark    = 562;
    static const unsigned kZeroSpace  = 562;
    static const unsigned kOneSpace   = 1687;
    static const unsigned kGapReset   = 100000;  // any pulse longer than this resets the machine

    // One pulse from the receiver. `mark` = true for a mark (carrier on / output LOW).
    // Returns true when a complete key event is available in `out`.
    bool feed(unsigned width_us, bool mark, NecKey& out);

    // Forget any partial frame (e.g. after the ring overflowed).
    void reset() { state_ = State::Idle; bits_ = 0; raw_ = 0; }

    // Diagnostics for the spike / log: frames rejected since construction.
    unsigned rejected() const { return rejected_; }

    // ±25 % window check, exposed for tests.
    static bool near(unsigned actual, unsigned expected) {
        unsigned lo = expected - expected / 4, hi = expected + expected / 4;
        return actual >= lo && actual <= hi;
    }

private:
    enum class State { Idle, LeadSpace, BitMark, BitSpace, TrailMark, RepeatMark };

    State    state_ = State::Idle;
    unsigned bits_ = 0;      // bits received so far in the current frame
    uint32_t raw_ = 0;       // bits accumulated LSB-first
    NecKey   last_{};        // last valid key, re-emitted on repeat frames
    bool     have_last_ = false;
    unsigned rejected_ = 0;

    bool finish(NecKey& out);
    void reject() { rejected_++; reset(); }
};

}  // namespace lf
