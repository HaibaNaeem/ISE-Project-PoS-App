#pragma once
// OWNER: Member 5. Resolves the active seasonal discount percent for a given date (no Qt).
#include <string>
#include <vector>
#include "Season.h"

class SeasonService {
public:
    explicit SeasonService(std::vector<Season> seasons = {});

    // Returns the highest active season percent where startDate <= date <= endDate, else 0.
    Percent percentFor(const std::string& date) const;

    const std::vector<Season>& seasons() const { return seasons_; }

private:
    std::vector<Season> seasons_;
};
