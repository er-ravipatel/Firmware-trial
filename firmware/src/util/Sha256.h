// SHA-256 — integrity primitive for OTA bundle verification (IMPLEMENTATION §1.3).
// Self-contained, no dependencies; usable on host (tests) and bare metal (device).
#pragma once
#include <cstddef>
#include <cstdint>

namespace lf {

class Sha256 {
public:
    static constexpr size_t kDigestSize = 32;

    Sha256() { reset(); }
    void reset();
    void update(const void* data, size_t len);
    void finish(uint8_t out[kDigestSize]);

    // One-shot convenience.
    static void hash(const void* data, size_t len, uint8_t out[kDigestSize]);

private:
    void process(const uint8_t block[64]);

    uint32_t state_[8];
    uint64_t bitlen_;
    uint8_t buf_[64];
    size_t buflen_;
};

}  // namespace lf
