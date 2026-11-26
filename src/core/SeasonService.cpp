// [WRITTEN BY MEMBER 4 on behalf of MEMBER 5 – minimal version so Member 4's billing work could build/test. Member 5: review, extend or replace; keep the agreed signatures.]
#include "SeasonService.h"

SeasonService::SeasonService(std::vector<Season> seasons)
    : seasons_(std::move(seasons)) {}

Percent SeasonService::percentFor(const std::string& date) const {
    Percent highest = 0;
    for (const auto& s : seasons_) {
        if (s.active && date >= s.startDate && date <= s.endDate) {
            if (s.percent > highest) {
                highest = s.percent;
            }
        }
    }
    return highest;
}
