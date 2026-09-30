#include "NecDecoder.h"

namespace lf {

bool NecDecoder::feed(unsigned w, bool mark, NecKey& out) {
    // A very long pulse in either polarity means the remote went quiet: start over. This is also
    // how the first mark after silence is treated as a lead mark regardless of prior state.
    if (w >= kGapReset) { reset(); return false; }

    switch (state_) {
    case State::Idle:
        // Waiting for the 9 ms lead mark. Spaces (idle line) are ignored here.
        if (mark && near(w, kLeadMark)) state_ = State::LeadSpace;
        else if (mark) reject();   // some other mark: noise, or a protocol we don't speak
        return false;

    case State::LeadSpace:
        if (mark) { reject(); return false; }
        if (near(w, kLeadSpace)) { state_ = State::BitMark; bits_ = 0; raw_ = 0; return false; }
        if (near(w, kRepeatSpace)) { state_ = State::RepeatMark; return false; }
        reject();
        return false;

    case State::RepeatMark:
        // Trailing 562 µs mark of a repeat frame. Only meaningful if we saw a key before.
        if (mark && near(w, kBitMark) && have_last_) {
            out = last_;
            out.repeat = true;
            state_ = State::Idle;
            return true;
        }
        reject();
        return false;

    case State::BitMark:
        if (mark && near(w, kBitMark)) { state_ = State::BitSpace; return false; }
        reject();
        return false;

    case State::BitSpace: {
        if (mark) { reject(); return false; }
        uint32_t bit;
        if (near(w, kZeroSpace)) bit = 0;
        else if (near(w, kOneSpace)) bit = 1;
        else { reject(); return false; }
        raw_ |= bit << bits_;
        bits_++;
        state_ = (bits_ == 32) ? State::TrailMark : State::BitMark;
        return false;
    }

    case State::TrailMark:
        if (mark && near(w, kBitMark)) {
            state_ = State::Idle;
            return finish(out);
        }
        reject();
        return false;
    }
    return false;
}

bool NecDecoder::finish(NecKey& out) {
    uint8_t a  = (uint8_t) (raw_ & 0xFF);
    uint8_t na = (uint8_t) ((raw_ >> 8) & 0xFF);
    uint8_t c  = (uint8_t) ((raw_ >> 16) & 0xFF);
    uint8_t nc = (uint8_t) ((raw_ >> 24) & 0xFF);

    // The command byte must be followed by its complement; that is what rejects sunlight and
    // LED-lamp noise that happens to look like a frame. The address may legitimately not be
    // complemented (extended NEC, 16-bit address) — accept it but flag it.
    if ((uint8_t) ~c != nc) { rejected_++; return false; }

    out.address  = a;
    out.command  = c;
    out.repeat   = false;
    out.extended = ((uint8_t) ~a != na);
    last_ = out;
    have_last_ = true;
    return true;
}

}  // namespace lf
