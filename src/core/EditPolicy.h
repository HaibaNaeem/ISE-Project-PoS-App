#pragma once
// OWNER: Member 3. Pure policy deciding whether a paid order can still be edited (7-day window).
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>

namespace EditPolicy {

namespace detail {

inline bool isLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

inline int daysInMonth(int y, int m) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 0;
    if (m == 2 && isLeapYear(y)) return 29;
    return kDays[m - 1];
}

inline bool parseIsoDate(const std::string& text, int& year, int& month, int& day) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return false;
    }
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }
    year = std::stoi(text.substr(0, 4));
    month = std::stoi(text.substr(5, 2));
    day = std::stoi(text.substr(8, 2));
    const int maxDay = daysInMonth(year, month);
    return maxDay > 0 && day >= 1 && day <= maxDay;
}

inline std::int64_t daysFromCivil(int y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yoe = static_cast<std::int64_t>(y) - era * 400;
    const std::int64_t adjM = static_cast<std::int64_t>(m) + (m > 2 ? -3 : 9);
    const std::int64_t doy = (153 * adjM + 2) / 5 + d - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

inline std::string formatCivilFromDays(std::int64_t z) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    std::int64_t y = yoe + era * 400;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;
    const std::int64_t m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2) ? 1 : 0;

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04lld-%02lld-%02lld",
                  static_cast<long long>(y),
                  static_cast<long long>(m),
                  static_cast<long long>(d));
    return std::string(buf);
}

} // namespace detail

inline constexpr std::int64_t kMaxEditDays = 7;

// TODO(M2): replace local daysBetween stub with DateText.h daysBetween when merged.
inline bool tryDaysBetween(const std::string& fromDate, const std::string& toDate, std::int64_t& outDays) {
    int y1 = 0, m1 = 0, d1 = 0;
    int y2 = 0, m2 = 0, d2 = 0;
    if (!detail::parseIsoDate(fromDate, y1, m1, d1) ||
        !detail::parseIsoDate(toDate, y2, m2, d2)) {
        return false;
    }
    outDays = detail::daysFromCivil(y2, m2, d2) - detail::daysFromCivil(y1, m1, d1);
    return true;
}

// Day 7 is included (daysBetween(billDate, today) <= 7 and >= 0).
inline bool withinEditWindow(const std::string& billDate, const std::string& today) {
    std::int64_t diff = 0;
    if (!tryDaysBetween(billDate, today, diff)) {
        return false;
    }
    return diff >= 0 && diff <= kMaxEditDays;
}

// Returns the last date (inclusive) on which a bill from billDate can be edited, or "" if invalid.
inline std::string editWindowUntilDate(const std::string& billDate) {
    int y = 0, m = 0, d = 0;
    if (!detail::parseIsoDate(billDate, y, m, d)) {
        return "";
    }
    return detail::formatCivilFromDays(detail::daysFromCivil(y, m, d) + kMaxEditDays);
}

} // namespace EditPolicy

using EditPolicy::withinEditWindow;
using EditPolicy::editWindowUntilDate;
