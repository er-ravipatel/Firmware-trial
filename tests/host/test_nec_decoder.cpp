#include "test_framework.h"
#include "input/NecDecoder.h"

#include <vector>
#include <utility>

using lf::NecDecoder;
using lf::NecKey;

namespace {

// A pulse: width in µs + mark flag. Helpers build the pulse train a remote would produce.
using Pulse = std::pair<unsigned, bool>;
constexpr bool MARK = true, SPACE = false;

// Build a full NEC frame for (address, command) with standard complements, optionally scaling
// every width by `scale` (1.0 = nominal) to simulate a fast/slow remote or IRQ jitter.
std::vector<Pulse> frame(uint8_t addr, uint8_t cmd, double scale = 1.0,
                         uint8_t addr_inv_override = 0, bool use_override = false) {
    std::vector<Pulse> p;
    auto push = [&](unsigned w, bool m) { p.push_back({unsigned(w * scale), m}); };
    push(NecDecoder::kLeadMark, MARK);
    push(NecDecoder::kLeadSpace, SPACE);
    uint8_t bytes[4] = {addr, use_override ? addr_inv_override : uint8_t(~addr),
                        cmd, uint8_t(~cmd)};
    for (uint8_t b : bytes) {
        for (int i = 0; i < 8; ++i) {
            push(NecDecoder::kBitMark, MARK);
            push(((b >> i) & 1) ? NecDecoder::kOneSpace : NecDecoder::kZeroSpace, SPACE);
        }
    }
    push(NecDecoder::kBitMark, MARK);   // trailing mark
    return p;
}

std::vector<Pulse> repeat_frame() {
    return {{NecDecoder::kLeadMark, MARK}, {NecDecoder::kRepeatSpace, SPACE},
            {NecDecoder::kBitMark, MARK}};
}

// Feed a train; return every key event emitted.
std::vector<NecKey> run(NecDecoder& d, const std::vector<Pulse>& train) {
    std::vector<NecKey> keys;
    for (auto& p : train) {
        NecKey k;
        if (d.feed(p.first, p.second, k)) keys.push_back(k);
    }
    return keys;
}

}  // namespace

TEST("nec: clean frame decodes address + command") {
    NecDecoder d;
    auto keys = run(d, frame(0x00, 0x45));   // 0x45 = "power" on the common kit remote
    CHECK_EQ(keys.size(), size_t(1));
    CHECK_EQ(int(keys[0].address), 0x00);
    CHECK_EQ(int(keys[0].command), 0x45);
    CHECK(!keys[0].repeat);
    CHECK(!keys[0].extended);
    CHECK_EQ(d.rejected(), 0u);
}

TEST("nec: all-ones and all-zeros commands round-trip") {
    NecDecoder d;
    auto k1 = run(d, frame(0xBF, 0xFF));
    auto k2 = run(d, frame(0xBF, 0x00));
    CHECK_EQ(k1.size(), size_t(1)); CHECK_EQ(int(k1[0].command), 0xFF);
    CHECK_EQ(k2.size(), size_t(1)); CHECK_EQ(int(k2[0].command), 0x00);
    CHECK_EQ(int(k2[0].address), 0xBF);
}

TEST("nec: repeat frames re-emit the last key with repeat=true") {
    NecDecoder d;
    auto train = frame(0x00, 0x46);
    // Key held: ~40 ms of silence then repeat frames every ~108 ms.
    train.push_back({40000, SPACE});
    auto r = repeat_frame();
    for (int i = 0; i < 3; ++i) {
        train.insert(train.end(), r.begin(), r.end());
        train.push_back({96000, SPACE});
    }
    auto keys = run(d, train);
    CHECK_EQ(keys.size(), size_t(4));
    CHECK(!keys[0].repeat);
    for (size_t i = 1; i < keys.size(); ++i) {
        CHECK(keys[i].repeat);
        CHECK_EQ(int(keys[i].command), 0x46);
    }
}

TEST("nec: a repeat frame with no prior key is ignored") {
    NecDecoder d;
    auto keys = run(d, repeat_frame());
    CHECK_EQ(keys.size(), size_t(0));
    CHECK_EQ(d.rejected(), 1u);
}

TEST("nec: +20% and -20% timing jitter still decode") {
    NecDecoder fast, slow;
    auto kf = run(fast, frame(0x10, 0xA5, 0.80));
    auto ks = run(slow, frame(0x10, 0xA5, 1.20));
    CHECK_EQ(kf.size(), size_t(1)); CHECK_EQ(int(kf[0].command), 0xA5);
    CHECK_EQ(ks.size(), size_t(1)); CHECK_EQ(int(ks[0].command), 0xA5);
}

TEST("nec: +40% timing is rejected, not misdecoded") {
    NecDecoder d;
    auto keys = run(d, frame(0x10, 0xA5, 1.40));
    CHECK_EQ(keys.size(), size_t(0));
    CHECK(d.rejected() >= 1u);
}

TEST("nec: corrupted command complement is rejected") {
    NecDecoder d;
    auto train = frame(0x00, 0x45);
    // Flip one bit of the ~command byte: bits 24..31 are pulses 2 + 2*24 .. (space index odd).
    // Bit 24 space is at index 2 + 24*2 + 1 = 51. Swap zero<->one space.
    auto& sp = train[51];
    sp.first = (sp.first == NecDecoder::kZeroSpace) ? NecDecoder::kOneSpace : NecDecoder::kZeroSpace;
    auto keys = run(d, train);
    CHECK_EQ(keys.size(), size_t(0));
    CHECK_EQ(d.rejected(), 1u);
}

TEST("nec: extended address (non-complementary) is accepted and flagged") {
    NecDecoder d;
    auto keys = run(d, frame(0x04, 0x08, 1.0, 0xFB ^ 0x01, true));  // ~0x04 = 0xFB; corrupt it
    CHECK_EQ(keys.size(), size_t(1));
    CHECK(keys[0].extended);
    CHECK_EQ(int(keys[0].command), 0x08);
}

TEST("nec: truncated frame followed by a good one decodes only the good one") {
    NecDecoder d;
    auto bad = frame(0x00, 0x45);
    bad.resize(20);                        // cut mid-frame
    bad.push_back({150000, SPACE});        // then silence (gap reset)
    auto good = frame(0x00, 0x47);
    bad.insert(bad.end(), good.begin(), good.end());
    auto keys = run(d, bad);
    CHECK_EQ(keys.size(), size_t(1));
    CHECK_EQ(int(keys[0].command), 0x47);
}

TEST("nec: random noise marks never produce a key") {
    NecDecoder d;
    std::vector<Pulse> noise;
    unsigned seed = 12345;
    for (int i = 0; i < 2000; ++i) {
        seed = seed * 1103515245u + 12345u;
        noise.push_back({100 + (seed >> 16) % 3000, (i & 1) == 0});
    }
    auto keys = run(d, noise);
    CHECK_EQ(keys.size(), size_t(0));
}

TEST("nec: two back-to-back presses of different keys both decode") {
    NecDecoder d;
    auto t = frame(0x00, 0x44);
    t.push_back({40000, SPACE});
    auto u = frame(0x00, 0x43);
    t.insert(t.end(), u.begin(), u.end());
    auto keys = run(d, t);
    CHECK_EQ(keys.size(), size_t(2));
    CHECK_EQ(int(keys[0].command), 0x44);
    CHECK_EQ(int(keys[1].command), 0x43);
}

TEST("nec: near() window is ±25%") {
    CHECK(NecDecoder::near(562, 562));
    CHECK(NecDecoder::near(562 * 3 / 4 + 1, 562));
    CHECK(NecDecoder::near(562 * 5 / 4 - 1, 562));
    CHECK(!NecDecoder::near(562 / 2, 562));
    CHECK(!NecDecoder::near(562 * 2, 562));
}
