// [WRITTEN BY MEMBER 4 on behalf of MEMBER 5 – minimal version so Member 4's billing work could build/test. Member 5: review, extend or replace; keep the agreed signatures.]
#pragma once
#include <string>
#include <cstdint>

#ifndef PERCENT_TYPE_DEFINED
#define PERCENT_TYPE_DEFINED
using Percent = std::int64_t; // hundredths of a percent: 8% = 800, 12.50% = 1250
#endif

struct Season {
    std::string startDate;
    std::string endDate;
    Percent percent = 0;
    bool active = false;
};
