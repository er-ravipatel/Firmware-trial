// SHA-256 correctness against known NIST test vectors.
#include "test_framework.h"
#include "util/Sha256.h"
#include <cstring>
#include <string>

using lf::Sha256;

static std::string hex(const uint8_t* d, size_t n) {
    static const char* k = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += k[d[i] >> 4];
        s += k[d[i] & 0xf];
    }
    return s;
}

static std::string sha_hex(const std::string& in) {
    uint8_t out[Sha256::kDigestSize];
    Sha256::hash(in.data(), in.size(), out);
    return hex(out, sizeof(out));
}

TEST("sha256 empty string") {
    CHECK_EQ(sha_hex(""),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST("sha256 abc") {
    CHECK_EQ(sha_hex("abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST("sha256 448-bit message") {
    CHECK_EQ(sha_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST("sha256 incremental update matches one-shot") {
    const std::string msg = "The quick brown fox jumps over the lazy dog";
    uint8_t one[32], inc[32];
    Sha256::hash(msg.data(), msg.size(), one);

    Sha256 h;
    h.update(msg.data(), 10);
    h.update(msg.data() + 10, msg.size() - 10);
    h.finish(inc);

    CHECK_EQ(hex(one, 32), hex(inc, 32));
    // Known digest for this classic input.
    CHECK_EQ(hex(one, 32),
             std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
}

TEST("sha256 spanning multiple blocks: incremental == one-shot, byte-by-byte") {
    std::string big(1000, 'a');  // > 15 x 64-byte blocks

    uint8_t oneshot[32];
    Sha256::hash(big.data(), big.size(), oneshot);

    // Feed one byte at a time — exercises buffering across block boundaries.
    Sha256 h;
    for (char ch : big) h.update(&ch, 1);
    uint8_t bybyte[32];
    h.finish(bybyte);

    CHECK_EQ(hex(oneshot, 32), hex(bybyte, 32));
}
