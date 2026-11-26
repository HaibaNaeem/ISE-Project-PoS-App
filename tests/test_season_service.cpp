// OWNER: Member 5. Unit tests for SeasonService.
#include "SeasonService.h"
#include "TestHelpers.h"

int main() {
    // Empty service returns 0
    SeasonService emptySvc;
    CHECK_EQ(emptySvc.percentFor("2026-11-25"), Percent(0));

    // Boundary dates (both start and end included), overlap (highest applies), inactive ignored
    std::vector<Season> seasons = {
        {"2026-11-20", "2026-11-30", 1000, true},  // 10.00% active
        {"2026-11-25", "2026-12-05", 1500, true},  // 15.00% active (overlaps Nov 25-30)
        {"2026-11-20", "2026-11-30", 2500, false}, // 25.00% inactive (must be ignored)
        {"2026-12-10", "2026-12-20", 2000, false}  // 20.00% inactive
    };
    SeasonService svc(seasons);

    // Outside active seasons
    CHECK_EQ(svc.percentFor("2026-11-19"), Percent(0));
    CHECK_EQ(svc.percentFor("2026-12-06"), Percent(0));
    CHECK_EQ(svc.percentFor("2026-12-15"), Percent(0)); // inactive season
    CHECK_EQ(svc.percentFor(""), Percent(0));
    CHECK_EQ(svc.percentFor("bad"), Percent(0));

    // Boundary start and end dates
    CHECK_EQ(svc.percentFor("2026-11-20"), Percent(1000)); // first day of season 1
    CHECK_EQ(svc.percentFor("2026-11-24"), Percent(1000)); // inside season 1 only
    CHECK_EQ(svc.percentFor("2026-11-25"), Percent(1500)); // overlap start: higher (15%), not 10+15=25%
    CHECK_EQ(svc.percentFor("2026-11-30"), Percent(1500)); // end of season 1 & overlap
    CHECK_EQ(svc.percentFor("2026-12-05"), Percent(1500)); // last day of season 2

    return testSummary("test_season_service");
}
