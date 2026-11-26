// OWNER: Member 5. Resolves the active seasonal discount percent for a given date (no Qt).
#include "SeasonService.h"
#include <cctype>

namespace {
bool isIsoDateShape(const std::string& d) {
    if (d.size() != 10) return false;
    for (size_t i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) {
            if (d[i] != '-') return false;
        } else if (!std::isdigit(static_cast<unsigned char>(d[i]))) {
            return false;
        }
    }
    return true;
}
} // namespace

SeasonService::SeasonService(std::vector<Season> seasons)
    : seasons_(std::move(seasons)) {}

Percent SeasonService::percentFor(const std::string& date) const {
    if (!isIsoDateShape(date)) return 0;

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
