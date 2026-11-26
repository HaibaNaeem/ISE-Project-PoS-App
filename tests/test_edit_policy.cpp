#include "EditPolicy.h"
#include "TestHelpers.h"

int main() {
    // Bill 2026-11-22: open on 11-29 yes (day 7), on 11-30 no (day 8)
    CHECK(withinEditWindow("2026-11-22", "2026-11-22"));
    CHECK(withinEditWindow("2026-11-22", "2026-11-29"));
    CHECK(!withinEditWindow("2026-11-22", "2026-11-30"));

    // Cross-year boundary: bill 2026-12-28, today 2027-01-04 yes (day 7), 2027-01-05 no (day 8)
    CHECK(withinEditWindow("2026-12-28", "2027-01-04"));
    CHECK(!withinEditWindow("2026-12-28", "2027-01-05"));

    // Leap year check: 2024 is leap year (Feb has 29 days)
    CHECK(withinEditWindow("2024-02-25", "2024-03-03"));
    CHECK(!withinEditWindow("2024-02-25", "2024-03-04"));

    // Non-leap year check: 2025-02-25 + 7 = 2025-03-04
    CHECK(withinEditWindow("2025-02-25", "2025-03-04"));
    CHECK(!withinEditWindow("2025-02-25", "2025-03-05"));

    // Future bill date or invalid date strings rejected
    CHECK(!withinEditWindow("2026-11-23", "2026-11-22"));
    CHECK(!withinEditWindow("", "2026-11-22"));
    CHECK(!withinEditWindow("2026-11-22", "bad-date"));
    CHECK(!withinEditWindow("2026-02-29", "2026-03-01"));

    // Deadline date helper
    CHECK_EQ(editWindowUntilDate("2026-11-22"), std::string("2026-11-29"));
    CHECK_EQ(editWindowUntilDate("2026-12-28"), std::string("2027-01-04"));
    CHECK_EQ(editWindowUntilDate("invalid"), std::string(""));

    return testSummary("test_edit_policy");
}
