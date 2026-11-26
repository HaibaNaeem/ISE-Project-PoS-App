#pragma once
// OWNER: Member 5. Persistence and validation for Seasons.txt (no Qt).
#include <cstddef>
#include <string>
#include <vector>
#include "CommonTypes.h"
#include "Season.h"

class SeasonStore {
public:
    OperationResult load(const std::string& path);
    OperationResult save(const std::string& path) const;
    std::string serialize() const;

    OperationResult add(const Season& season);
    OperationResult setActive(std::size_t index, bool active);

    const std::vector<Season>& all() const { return seasons_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    std::vector<Season> seasons_;
    std::vector<std::string> warnings_;
};
