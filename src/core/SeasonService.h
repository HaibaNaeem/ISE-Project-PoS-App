// [WRITTEN BY MEMBER 4 on behalf of MEMBER 5 – minimal version so Member 4's billing work could build/test. Member 5: review, extend or replace; keep the agreed signatures.]
#pragma once
#include <string>
#include <vector>
#include "Season.h"

class SeasonService {
public:
    explicit SeasonService(std::vector<Season> seasons = {});
    Percent percentFor(const std::string& date) const;

private:
    std::vector<Season> seasons_;
};
