#include "test_framework.h"
#include "app/PluginScheduler.h"

using lf::PluginScheduler;
using lf::PluginSlot;

TEST("scheduler round-robins enabled always-on plugins") {
    PluginScheduler<8> sch;
    CHECK(sch.add({"photo"}).is_ok());
    CHECK(sch.add({"clock"}).is_ok());
    CHECK(sch.add({"weather"}).is_ok());

    int now = 600;  // 10:00, irrelevant for always-on
    CHECK_EQ(sch.next(now), 0);  // photo
    CHECK_EQ(sch.next(now), 1);  // clock
    CHECK_EQ(sch.next(now), 2);  // weather
    CHECK_EQ(sch.next(now), 0);  // wraps back to photo
}

TEST("scheduler skips disabled plugins") {
    PluginScheduler<8> sch;
    sch.add({"photo"});
    sch.add({"clock"});
    sch.add({"weather"});
    CHECK(sch.set_enabled("clock", false));

    int now = 600;
    CHECK_EQ(sch.next(now), 0);  // photo
    CHECK_EQ(sch.next(now), 2);  // clock skipped -> weather
    CHECK_EQ(sch.next(now), 0);  // wrap
}

TEST("scheduler honors time windows") {
    PluginScheduler<8> sch;
    // weather only 06:00-09:00 (360..540)
    sch.add({"photo"});
    sch.add({"weather", true, 30, 360, 540});

    CHECK_EQ(sch.next(400), 0);  // 06:40 photo
    CHECK_EQ(sch.next(400), 1);  // weather active at 06:40

    // At 10:00 (600) weather is outside its window -> only photo shows.
    CHECK_EQ(sch.next(600), 0);
    CHECK_EQ(sch.next(600), 0);  // weather skipped, stays on photo
}

TEST("scheduler window wraps midnight") {
    // night-clock active 22:00-06:00 (1320..360, wraps)
    CHECK(PluginScheduler<2>::active_at({"nite", true, 30, 1320, 360}, 1380));  // 23:00 yes
    CHECK(PluginScheduler<2>::active_at({"nite", true, 30, 1320, 360}, 60));    // 01:00 yes
    CHECK(!PluginScheduler<2>::active_at({"nite", true, 30, 1320, 360}, 720));  // 12:00 no
}

TEST("scheduler returns -1 when nothing active") {
    PluginScheduler<4> sch;
    sch.add({"weather", true, 30, 360, 540});  // only morning
    CHECK_EQ(sch.next(720), -1);               // noon -> nothing showable

    PluginScheduler<4> empty;
    CHECK_EQ(empty.next(0), -1);
}

TEST("scheduler add respects capacity") {
    PluginScheduler<2> sch;
    CHECK(sch.add({"a"}).is_ok());
    CHECK(sch.add({"b"}).is_ok());
    CHECK(sch.add({"c"}).is_err());  // full
    CHECK(sch.add({"c"}).error() == lf::Error::Full);
    CHECK_EQ(sch.count(), size_t(2));
}
