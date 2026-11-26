#pragma once
// OWNER: Member 5. Shared contract for seasonal discounts (no Qt).
#include <cstdint>
#include <string>

#ifndef PERCENT_TYPE_DEFINED
#define PERCENT_TYPE_DEFINED
using Percent = std::int64_t; // hundredths of a percent: 8% = 800, 12.50% = 1250
#endif

struct Season {
    std::string startDate; // YYYY-MM-DD
    std::string endDate;   // YYYY-MM-DD
    Percent percent = 0;   // > 0 and <= 10000 (100.00%)
    bool active = false;
};
