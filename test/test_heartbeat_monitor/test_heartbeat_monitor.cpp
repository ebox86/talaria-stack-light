#include <unity.h>

#include "heartbeat_monitor.h"

void setUp() {}
void tearDown() {}

static void test_never_contacted_is_stale() {
    HeartbeatMonitor monitor(30000);
    TEST_ASSERT_TRUE(monitor.isStale(0));
    TEST_ASSERT_TRUE(monitor.isStale(999999));
    TEST_ASSERT_EQUAL(-1, monitor.msSinceContact(0));
}

static void test_fresh_contact_is_not_stale() {
    HeartbeatMonitor monitor(30000);
    monitor.markContact(1000);

    TEST_ASSERT_FALSE(monitor.isStale(1000));
    TEST_ASSERT_FALSE(monitor.isStale(1000 + 29999));
    TEST_ASSERT_EQUAL(29999, monitor.msSinceContact(1000 + 29999));
}

static void test_contact_becomes_stale_exactly_at_timeout() {
    HeartbeatMonitor monitor(30000);
    monitor.markContact(1000);

    TEST_ASSERT_TRUE(monitor.isStale(1000 + 30000));
}

static void test_repeated_contact_keeps_resetting_the_window() {
    HeartbeatMonitor monitor(30000);
    monitor.markContact(1000);
    monitor.markContact(20000);

    // Would have gone stale relative to the first contact (1000 + 30000
    // = 31000), but the second contact should have reset the window.
    TEST_ASSERT_FALSE(monitor.isStale(31000));
    TEST_ASSERT_EQUAL(11000, monitor.msSinceContact(31000));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_never_contacted_is_stale);
    RUN_TEST(test_fresh_contact_is_not_stale);
    RUN_TEST(test_contact_becomes_stale_exactly_at_timeout);
    RUN_TEST(test_repeated_contact_keeps_resetting_the_window);

    return UNITY_END();
}
