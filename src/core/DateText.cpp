#include "DateText.h"
#include <cstdio>
#include <ctime>

namespace {
long long daysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void civilFromDays(long long z, int& y, int& m, int& d)
{
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = static_cast<unsigned>(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = static_cast<long long>(yoe) + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yy + (m <= 2));
}

bool parseDate(const std::string& s, int& y, int& m, int& d)
{
    return std::sscanf(s.c_str(), "%4d-%2d-%2d", &y, &m, &d) == 3;
}

std::string pad2(int v) { return (v < 10 ? "0" : "") + std::to_string(v); }
}

std::string todayText()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    return std::to_string(tm.tm_year + 1900) + "-" + pad2(tm.tm_mon + 1) + "-" + pad2(tm.tm_mday);
}

std::string nowTimeText()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    return pad2(tm.tm_hour) + ":" + pad2(tm.tm_min) + ":" + pad2(tm.tm_sec);
}

int daysBetween(const std::string& from, const std::string& to)
{
    int y1, m1, d1, y2, m2, d2;
    if (!parseDate(from, y1, m1, d1) || !parseDate(to, y2, m2, d2)) return 0;
    return static_cast<int>(daysFromCivil(y2, m2, d2) - daysFromCivil(y1, m1, d1));
}

std::string addDays(const std::string& date, int n)
{
    int y, m, d;
    if (!parseDate(date, y, m, d)) return date;
    civilFromDays(daysFromCivil(y, m, d) + n, y, m, d);
    return std::to_string(y) + "-" + pad2(m) + "-" + pad2(d);
}