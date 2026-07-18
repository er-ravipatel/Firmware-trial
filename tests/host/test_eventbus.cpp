#include "test_framework.h"
#include "core/EventBus.h"

using lf::Event;
using lf::EventBus;
using lf::EventType;

TEST("eventbus posts and polls in order") {
    EventBus<8> bus;
    CHECK(bus.post({EventType::CmdNext, 0}));
    CHECK(bus.post({EventType::ImportDone, 5}));

    Event e;
    CHECK(bus.poll(e));
    CHECK(e.type == EventType::CmdNext);

    CHECK(bus.poll(e));
    CHECK(e.type == EventType::ImportDone);
    CHECK_EQ(e.arg, 5);

    CHECK(!bus.poll(e));  // drained
}

TEST("eventbus post returns false when full, lossy never blocks") {
    EventBus<2> bus;
    CHECK(bus.post({EventType::DwellExpire, 0}));
    CHECK(bus.post({EventType::DwellExpire, 0}));
    CHECK(!bus.post({EventType::DwellExpire, 0}));  // full

    // Lossy variant drops oldest and always accepts.
    bus.post_lossy({EventType::Motion, 42});
    Event e;
    CHECK(bus.poll(e));
    // The first DwellExpire was dropped; a DwellExpire and the Motion remain.
    CHECK(bus.poll(e));
    CHECK(e.type == EventType::Motion);
    CHECK_EQ(e.arg, 42);
}
