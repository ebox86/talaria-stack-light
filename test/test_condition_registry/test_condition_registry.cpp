#include <unity.h>

#include "condition_registry.h"

void setUp() {}
void tearDown() {}

static Condition makeCondition(const std::string& source, const std::string& conditionName,
                                SignalSeverity severity, unsigned long ttlSeconds = 0,
                                const std::string& message = "") {
    Condition c;
    c.source = source;
    c.condition = conditionName;
    c.severity = severity;
    c.message = message;
    c.ttlSeconds = ttlSeconds;
    return c;
}

static void test_empty_registry_has_no_highest_priority() {
    ConditionRegistry registry(10);
    TEST_ASSERT_NULL(registry.highestPriority());
    TEST_ASSERT_EQUAL(0, registry.count());
}

static void test_upsert_adds_a_new_condition() {
    ConditionRegistry registry(10);
    TEST_ASSERT_TRUE(registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::WARNING), 1000));

    TEST_ASSERT_EQUAL(1, registry.count());
    const Condition* top = registry.highestPriority();
    TEST_ASSERT_NOT_NULL(top);
    TEST_ASSERT_EQUAL_STRING("printer-front-01", top->source.c_str());
    TEST_ASSERT_EQUAL_STRING("PAPER_JAM", top->condition.c_str());
}

static void test_upsert_same_source_and_condition_replaces_not_duplicates() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::WARNING, 0, "first"), 1000);
    registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::CRITICAL, 0, "second"), 2000);

    TEST_ASSERT_EQUAL(1, registry.count());
    const Condition* top = registry.highestPriority();
    TEST_ASSERT_EQUAL(static_cast<int>(SignalSeverity::CRITICAL), static_cast<int>(top->severity));
    TEST_ASSERT_EQUAL_STRING("second", top->message.c_str());
}

static void test_highest_priority_picks_the_most_severe_active_condition() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("store", "OPEN_NOTE", SignalSeverity::INFO), 1000);
    registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::WARNING), 1000);
    registry.upsert(makeCondition("network", "LINK_DOWN", SignalSeverity::CRITICAL), 1000);

    const Condition* top = registry.highestPriority();
    TEST_ASSERT_NOT_NULL(top);
    TEST_ASSERT_EQUAL_STRING("network", top->source.c_str());
    TEST_ASSERT_EQUAL_STRING("LINK_DOWN", top->condition.c_str());
}

static void test_removing_the_top_condition_reveals_the_next_one() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::WARNING), 1000);
    registry.upsert(makeCondition("network", "LINK_DOWN", SignalSeverity::CRITICAL), 1000);

    bool removed = registry.remove("network", "LINK_DOWN");
    TEST_ASSERT_TRUE(removed);

    const Condition* top = registry.highestPriority();
    TEST_ASSERT_NOT_NULL(top);
    TEST_ASSERT_EQUAL_STRING("printer-front-01", top->source.c_str());
}

static void test_remove_unknown_condition_returns_false() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::WARNING), 1000);

    TEST_ASSERT_FALSE(registry.remove("printer-front-01", "NOT_A_REAL_CONDITION"));
    TEST_ASSERT_EQUAL(1, registry.count());
}

static void test_clear_removes_everything() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("a", "x", SignalSeverity::INFO), 1000);
    registry.upsert(makeCondition("b", "y", SignalSeverity::CRITICAL), 1000);

    registry.clear();

    TEST_ASSERT_EQUAL(0, registry.count());
    TEST_ASSERT_NULL(registry.highestPriority());
}

static void test_zero_ttl_never_expires() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("a", "x", SignalSeverity::WARNING, 0), 1000);

    registry.pruneExpired(1000UL + 365UL * 24 * 3600 * 1000UL); // a year later
    TEST_ASSERT_EQUAL(1, registry.count());
}

static void test_condition_expires_after_its_ttl_elapses() {
    ConditionRegistry registry(10);
    // 10 second TTL, created at t=1000ms.
    registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::WARNING, 10), 1000);

    registry.pruneExpired(1000 + 9000); // 9s later -- not expired yet
    TEST_ASSERT_EQUAL(1, registry.count());

    registry.pruneExpired(1000 + 10000); // exactly at the TTL -- expired
    TEST_ASSERT_EQUAL(0, registry.count());
    TEST_ASSERT_NULL(registry.highestPriority());
}

static void test_re_upserting_resets_the_ttl_clock() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("a", "x", SignalSeverity::WARNING, 10), 1000);

    // Refresh at t=9000 (before the original 11000ms expiry).
    registry.upsert(makeCondition("a", "x", SignalSeverity::WARNING, 10), 9000);

    // Would have expired relative to the *first* upsert (1000 + 10000 =
    // 11000), but the refresh should have pushed expiry out to 19000.
    registry.pruneExpired(11000);
    TEST_ASSERT_EQUAL(1, registry.count());

    registry.pruneExpired(19000);
    TEST_ASSERT_EQUAL(0, registry.count());
}

static void test_expiring_the_top_condition_reveals_the_next_one() {
    ConditionRegistry registry(10);
    registry.upsert(makeCondition("printer-front-01", "PAPER_JAM", SignalSeverity::WARNING, 0), 1000);
    registry.upsert(makeCondition("network", "LINK_DOWN", SignalSeverity::CRITICAL, 5), 1000);

    registry.pruneExpired(1000 + 5000);

    const Condition* top = registry.highestPriority();
    TEST_ASSERT_NOT_NULL(top);
    TEST_ASSERT_EQUAL_STRING("printer-front-01", top->source.c_str());
}

static void test_upsert_rejects_new_entries_once_at_capacity() {
    ConditionRegistry registry(2);
    TEST_ASSERT_TRUE(registry.upsert(makeCondition("a", "x", SignalSeverity::INFO), 1000));
    TEST_ASSERT_TRUE(registry.upsert(makeCondition("b", "y", SignalSeverity::INFO), 1000));

    // Registry is full -- a brand-new (source, condition) pair is rejected.
    TEST_ASSERT_FALSE(registry.upsert(makeCondition("c", "z", SignalSeverity::INFO), 1000));
    TEST_ASSERT_EQUAL(2, registry.count());
}

static void test_upsert_refreshing_an_existing_entry_ignores_the_cap() {
    ConditionRegistry registry(1);
    TEST_ASSERT_TRUE(registry.upsert(makeCondition("a", "x", SignalSeverity::INFO), 1000));

    // Already at capacity (1/1), but this refreshes the *same* key, so it
    // must still succeed.
    TEST_ASSERT_TRUE(registry.upsert(makeCondition("a", "x", SignalSeverity::CRITICAL), 2000));
    TEST_ASSERT_EQUAL(1, registry.count());
    TEST_ASSERT_EQUAL(static_cast<int>(SignalSeverity::CRITICAL),
                       static_cast<int>(registry.highestPriority()->severity));
}

static void test_removing_an_entry_frees_a_capacity_slot() {
    ConditionRegistry registry(1);
    registry.upsert(makeCondition("a", "x", SignalSeverity::INFO), 1000);
    TEST_ASSERT_FALSE(registry.upsert(makeCondition("b", "y", SignalSeverity::INFO), 1000));

    registry.remove("a", "x");
    TEST_ASSERT_TRUE(registry.upsert(makeCondition("b", "y", SignalSeverity::INFO), 1000));
}

static void test_signal_for_severity_matches_expected_visuals() {
    SignalState info = signalForSeverity(SignalSeverity::INFO);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::YELLOW), static_cast<int>(info.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::SOLID), static_cast<int>(info.pattern));

    SignalState warning = signalForSeverity(SignalSeverity::WARNING);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::YELLOW), static_cast<int>(warning.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::SLOW_FLASH), static_cast<int>(warning.pattern));

    SignalState critical = signalForSeverity(SignalSeverity::CRITICAL);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalColor::RED), static_cast<int>(critical.color));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::FAST_FLASH), static_cast<int>(critical.pattern));

    SignalState emergency = signalForSeverity(SignalSeverity::EMERGENCY);
    TEST_ASSERT_EQUAL(static_cast<int>(SignalPattern::ALTERNATING), static_cast<int>(emergency.pattern));

    // Severity priorities must be strictly increasing, since that ordering
    // is what highestPriority() relies on.
    TEST_ASSERT_TRUE(info.priority < warning.priority);
    TEST_ASSERT_TRUE(warning.priority < critical.priority);
    TEST_ASSERT_TRUE(critical.priority < emergency.priority);
}

static void test_severity_from_string_parses_known_values_case_insensitively() {
    SignalSeverity severity;

    TEST_ASSERT_TRUE(severityFromString("warning", severity));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalSeverity::WARNING), static_cast<int>(severity));

    TEST_ASSERT_TRUE(severityFromString("CRITICAL", severity));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalSeverity::CRITICAL), static_cast<int>(severity));

    TEST_ASSERT_TRUE(severityFromString("Emergency", severity));
    TEST_ASSERT_EQUAL(static_cast<int>(SignalSeverity::EMERGENCY), static_cast<int>(severity));
}

static void test_severity_from_string_rejects_unknown_values() {
    SignalSeverity severity;
    TEST_ASSERT_FALSE(severityFromString("catastrophic", severity));
    TEST_ASSERT_FALSE(severityFromString("", severity));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_registry_has_no_highest_priority);
    RUN_TEST(test_upsert_adds_a_new_condition);
    RUN_TEST(test_upsert_same_source_and_condition_replaces_not_duplicates);
    RUN_TEST(test_highest_priority_picks_the_most_severe_active_condition);
    RUN_TEST(test_removing_the_top_condition_reveals_the_next_one);
    RUN_TEST(test_remove_unknown_condition_returns_false);
    RUN_TEST(test_clear_removes_everything);
    RUN_TEST(test_zero_ttl_never_expires);
    RUN_TEST(test_condition_expires_after_its_ttl_elapses);
    RUN_TEST(test_re_upserting_resets_the_ttl_clock);
    RUN_TEST(test_expiring_the_top_condition_reveals_the_next_one);
    RUN_TEST(test_upsert_rejects_new_entries_once_at_capacity);
    RUN_TEST(test_upsert_refreshing_an_existing_entry_ignores_the_cap);
    RUN_TEST(test_removing_an_entry_frees_a_capacity_slot);
    RUN_TEST(test_signal_for_severity_matches_expected_visuals);
    RUN_TEST(test_severity_from_string_parses_known_values_case_insensitively);
    RUN_TEST(test_severity_from_string_rejects_unknown_values);

    return UNITY_END();
}
