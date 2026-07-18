#include "test_framework.h"
#include "util/RingBuffer.h"

using lf::RingBuffer;

TEST("ringbuffer basic push/pop FIFO order") {
    RingBuffer<int, 4> rb;
    CHECK(rb.empty());
    CHECK(rb.push(1));
    CHECK(rb.push(2));
    CHECK(rb.push(3));
    CHECK_EQ(rb.size(), size_t(3));

    int v = 0;
    CHECK(rb.pop(v)); CHECK_EQ(v, 1);
    CHECK(rb.pop(v)); CHECK_EQ(v, 2);
    CHECK(rb.pop(v)); CHECK_EQ(v, 3);
    CHECK(rb.empty());
    CHECK(!rb.pop(v));  // empty pop fails
}

TEST("ringbuffer rejects push when full") {
    RingBuffer<int, 2> rb;
    CHECK(rb.push(10));
    CHECK(rb.push(20));
    CHECK(rb.full());
    CHECK(!rb.push(30));  // full -> false, no overwrite
    int v = 0;
    CHECK(rb.pop(v)); CHECK_EQ(v, 10);  // oldest preserved
}

TEST("ringbuffer push_overwrite drops oldest") {
    RingBuffer<int, 2> rb;
    rb.push_overwrite(1);
    rb.push_overwrite(2);
    rb.push_overwrite(3);  // drops 1
    CHECK_EQ(rb.size(), size_t(2));
    int v = 0;
    CHECK(rb.pop(v)); CHECK_EQ(v, 2);
    CHECK(rb.pop(v)); CHECK_EQ(v, 3);
}

TEST("ringbuffer wraps around correctly") {
    RingBuffer<int, 3> rb;
    int v = 0;
    for (int i = 0; i < 100; ++i) {
        CHECK(rb.push(i));
        CHECK(rb.pop(v));
        CHECK_EQ(v, i);
    }
    CHECK(rb.empty());
}
