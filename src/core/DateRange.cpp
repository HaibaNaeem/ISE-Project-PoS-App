// OWNER: Member 5. Date range presets and filtering for manager reports (no Qt).
#include "DateRange.h"
#include "DateText.h"

DateRange rangeFor(Period period, const std::string& today) {
    switch (period) {
        case Period::Today:
            return {today, today};
        case Period::Last7Days:
            return {addDays(today, -6), today};
        case Period::ThisMonth:
            if (today.size() == 10) {
                return {today.substr(0, 7) + "-01", today};
            }
            return {today, today};
        case Period::ThisYear:
            if (today.size() == 10) {
                return {today.substr(0, 4) + "-01-01", today};
            }
            return {today, today};
    }
    return {today, today};
}

bool inDateRange(const std::string& date, const DateRange& range) {
    if (date.empty()) return false;
    if (!range.from.empty() && date < range.from) return false;
    if (!range.to.empty() && date > range.to) return false;
    return true;
}
