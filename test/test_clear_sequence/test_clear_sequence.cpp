#include <unity.h>
#include "clear_sequence.h"
void setUp() {}
void tearDown() {}
static void one_pass() {
    ClearSequence sequence;
    sequence.start(1000);
    TEST_ASSERT_EQUAL(SignalColor::GREEN, sequence.update(1000));
    TEST_ASSERT_EQUAL(SignalColor::YELLOW, sequence.update(1400));
    TEST_ASSERT_EQUAL(SignalColor::RED, sequence.update(1800));
    TEST_ASSERT_EQUAL(SignalColor::NONE, sequence.update(2200));
    TEST_ASSERT_FALSE(sequence.active());
    TEST_ASSERT_EQUAL(SignalColor::NONE, sequence.update(2600));
}
static void cancel_for_new_alert() {
    ClearSequence sequence;
    sequence.start(0);
    sequence.cancel();
    TEST_ASSERT_FALSE(sequence.active());
    TEST_ASSERT_EQUAL(SignalColor::NONE, sequence.update(400));
}
static void clock_wrap() {
    ClearSequence sequence;
    sequence.start(UINT32_MAX - 199);
    TEST_ASSERT_EQUAL(SignalColor::YELLOW, sequence.update(200));
    TEST_ASSERT_EQUAL(SignalColor::NONE, sequence.update(1000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(one_pass);
    RUN_TEST(cancel_for_new_alert);
    RUN_TEST(clock_wrap);
    return UNITY_END();
}
