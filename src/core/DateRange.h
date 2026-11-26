#pragma once
// OWNER: Member 5. Date range presets and filtering for manager reports (no Qt).
#include <string>

enum class Period {
    Today,
    Last7Days,
    ThisMonth,
    ThisYear
};

struct DateRange {
    std::string from; // inclusive YYYY-MM-DD
    std::string to;   // inclusive YYYY-MM-DD
};

DateRange rangeFor(Period period, const std::string& today);
bool inDateRange(const std::string& date, const DateRange& range);
