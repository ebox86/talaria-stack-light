#include <unity.h>

#include "status_engine.h"

void setUp() {}
void tearDown() {}

static void test_open_is_green_solid_normal() {
    SignalState state = signalForStatus(TalariaStatus::OPEN);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::GREEN), static_cast<int>(state.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::SOLID), static_cast<int>(state.pattern));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPriority::NORMAL), static_cast<int>(state.priority));
}

static void test_closed_is_red_solid_normal() {
    SignalState state = signalForStatus(TalariaStatus::CLOSED);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::RED), static_cast<int>(state.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::SOLID), static_cast<int>(state.pattern));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPriority::NORMAL), static_cast<int>(state.priority));
}

static void test_warning_is_yellow_slow_flash() {
    SignalState state = signalForStatus(TalariaStatus::WARNING);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::YELLOW), static_cast<int>(state.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::SLOW_FLASH), static_cast<int>(state.pattern));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPriority::WARNING), static_cast<int>(state.priority));
}

static void test_critical_is_red_fast_flash() {
    SignalState state = signalForStatus(TalariaStatus::CRITICAL);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::RED), static_cast<int>(state.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::FAST_FLASH), static_cast<int>(state.pattern));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPriority::CRITICAL), static_cast<int>(state.priority));
}

static void test_offline_is_yellow_fast_flash_and_critical_priority() {
    // OFFLINE means "we've lost contact with Talaria" -- it should read as
    // at least as urgent as CRITICAL so it can't be silently ignored.
    SignalState state = signalForStatus(TalariaStatus::OFFLINE);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::YELLOW), static_cast<int>(state.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::FAST_FLASH), static_cast<int>(state.pattern));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPriority::CRITICAL), static_cast<int>(state.priority));
}

static void test_status_names_round_trip_expected_strings() {
    TEST_ASSERT_EQUAL_STRING("open", toString(TalariaStatus::OPEN));
    TEST_ASSERT_EQUAL_STRING("closed", toString(TalariaStatus::CLOSED));
    TEST_ASSERT_EQUAL_STRING("warning", toString(TalariaStatus::WARNING));
    TEST_ASSERT_EQUAL_STRING("critical", toString(TalariaStatus::CRITICAL));
    TEST_ASSERT_EQUAL_STRING("offline", toString(TalariaStatus::OFFLINE));
}

static void test_color_and_pattern_names_round_trip_expected_strings() {
    TEST_ASSERT_EQUAL_STRING("red", toString(SignalColor::RED));
    TEST_ASSERT_EQUAL_STRING("yellow", toString(SignalColor::YELLOW));
    TEST_ASSERT_EQUAL_STRING("green", toString(SignalColor::GREEN));
    TEST_ASSERT_EQUAL_STRING("none", toString(SignalColor::NONE));

    TEST_ASSERT_EQUAL_STRING("solid", toString(SignalPattern::SOLID));
    TEST_ASSERT_EQUAL_STRING("slow_flash", toString(SignalPattern::SLOW_FLASH));
    TEST_ASSERT_EQUAL_STRING("fast_flash", toString(SignalPattern::FAST_FLASH));
    TEST_ASSERT_EQUAL_STRING("off", toString(SignalPattern::OFF));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_open_is_green_solid_normal);
    RUN_TEST(test_closed_is_red_solid_normal);
    RUN_TEST(test_warning_is_yellow_slow_flash);
    RUN_TEST(test_critical_is_red_fast_flash);
    RUN_TEST(test_offline_is_yellow_fast_flash_and_critical_priority);
    RUN_TEST(test_status_names_round_trip_expected_strings);
    RUN_TEST(test_color_and_pattern_names_round_trip_expected_strings);

    return UNITY_END();
}
