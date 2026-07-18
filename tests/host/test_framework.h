// Tiny zero-dependency test framework for host-side unit tests.
// Auto-registers TEST(name){...} blocks; CHECK/CHECK_EQ record failures.
#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace lftest {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }

struct Registrar {
    Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

}  // namespace lftest

#define LF_CAT2(a, b) a##b
#define LF_CAT(a, b) LF_CAT2(a, b)

#define TEST(name)                                                             \
    static void LF_CAT(lf_test_fn_, __LINE__)();                               \
    static ::lftest::Registrar LF_CAT(lf_test_reg_, __LINE__)(                 \
        name, LF_CAT(lf_test_fn_, __LINE__));                                  \
    static void LF_CAT(lf_test_fn_, __LINE__)()

#define CHECK(cond)                                                            \
    do {                                                                       \
        ::lftest::checks()++;                                                  \
        if (!(cond)) {                                                         \
            ::lftest::failures()++;                                            \
            std::printf("    FAIL %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond); \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        ::lftest::checks()++;                                                  \
        if (!((a) == (b))) {                                                   \
            ::lftest::failures()++;                                            \
            std::printf("    FAIL %s:%d  CHECK_EQ(%s, %s)\n", __FILE__, __LINE__, #a, #b); \
        }                                                                      \
    } while (0)
