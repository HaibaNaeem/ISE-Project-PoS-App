#include <iostream>
#include "DateText.h"

static int failed = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "FAIL line " << __LINE__ << ": " #c "\n"; ++failed; } } while (0)

int main()
{
    CHECK(daysBetween("2026-11-22", "2026-11-29") == 7);
    CHECK(daysBetween("2026-11-22", "2026-11-30") == 8);
    CHECK(daysBetween("2026-11-22", "2026-11-22") == 0);
    CHECK(daysBetween("2026-12-28", "2027-01-04") == 7);
    CHECK(daysBetween("2027-01-04", "2026-12-28") == -7);
    CHECK(addDays("2028-02-28", 1) == "2028-02-29");     // leap year
    CHECK(addDays("2027-02-28", 1) == "2027-03-01");
    CHECK(addDays("2026-11-30", 1) == "2026-12-01");     // month end
    CHECK(addDays("2026-12-31", 1) == "2027-01-01");     // year end
    CHECK(addDays("2026-12-28", 7) == "2027-01-04");
    CHECK(addDays("2026-11-22", -2) == "2026-11-20");
    CHECK(todayText().size() == 10);
    CHECK(nowTimeText().size() == 8);
    std::cout << (failed ? "FAILED\n" : "All date tests passed\n");
    return failed ? 1 : 0;
}