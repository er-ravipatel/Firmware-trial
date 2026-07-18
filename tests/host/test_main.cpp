#include "test_framework.h"

int main() {
    int prev = 0;
    for (auto& c : lftest::registry()) {
        std::printf("[ RUN  ] %s\n", c.name);
        prev = lftest::failures();
        c.fn();
        bool ok = (lftest::failures() == prev);
        std::printf("[ %s ] %s\n", ok ? "PASS" : "FAIL", c.name);
    }
    std::printf("\n%d checks, %d failures\n", lftest::checks(), lftest::failures());
    return lftest::failures() ? 1 : 0;
}
