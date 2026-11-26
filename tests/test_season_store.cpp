// OWNER: Member 5. Unit tests for SeasonStore.
#include "SeasonStore.h"
#include "TestHelpers.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static void removeTempFile(const fs::path& p) {
    std::error_code ec;
    fs::remove(p, ec);
}

int main() {
    const fs::path tempDir = fs::temp_directory_path();
    const fs::path seasonFile = tempDir / "test_seasons_store.txt";
    const fs::path badFile = tempDir / "test_seasons_bad.txt";
    removeTempFile(seasonFile);
    removeTempFile(badFile);

    // 1. Add validation: valid dates, leap year, start <= end, percent > 0 and <= 100.00, overlaps allowed
    SeasonStore store;
    CHECK(store.add({"2026-11-20", "2026-11-30", 1000, true}).success);
    CHECK(store.add({"2026-11-25", "2026-12-05", 1500, true}).success); // overlap allowed
    CHECK(store.add({"2024-02-29", "2024-02-29", 1, false}).success);   // leap year & min percent 0.01%
    CHECK(store.add({"2026-01-01", "2026-01-31", 10000, true}).success); // max percent 100.00%

    // Invalid cases rejected
    CHECK(!store.add({"2025-02-29", "2025-03-01", 1000, true}).success);  // non-leap year Feb 29
    CHECK(!store.add({"2026-04-31", "2026-05-01", 1000, true}).success);  // April has 30 days
    CHECK(!store.add({"2026-13-01", "2026-13-10", 1000, true}).success);  // month 13
    CHECK(!store.add({"2026-11-20", "bad-date", 1000, true}).success);    // bad format
    CHECK(!store.add({"2026-11-30", "2026-11-20", 1000, true}).success);  // start > end
    CHECK(!store.add({"2026-11-20", "2026-11-30", 0, true}).success);     // percent must be > 0
    CHECK(!store.add({"2026-11-20", "2026-11-30", -500, true}).success);  // negative percent
    CHECK(!store.add({"2026-11-20", "2026-11-30", 10001, true}).success); // > 100.00%

    CHECK_EQ(store.all().size(), static_cast<std::size_t>(4));

    // 2. setActive
    CHECK(store.setActive(0, false).success);
    CHECK(!store.all()[0].active);
    CHECK(store.setActive(0, true).success);
    CHECK(store.all()[0].active);
    CHECK(!store.setActive(99, true).success);

    // 3. Save and reload round trip identically
    CHECK(store.save(seasonFile.string()).success);

    SeasonStore reloaded;
    OperationResult loadOk = reloaded.load(seasonFile.string());
    CHECK(loadOk.success);
    CHECK_EQ(reloaded.warnings().size(), static_cast<std::size_t>(0));
    CHECK_EQ(reloaded.all().size(), static_cast<std::size_t>(4));
    CHECK_EQ(reloaded.all()[0].startDate, std::string("2026-11-20"));
    CHECK_EQ(reloaded.all()[0].endDate, std::string("2026-11-30"));
    CHECK_EQ(reloaded.all()[0].percent, Percent(1000));
    CHECK(reloaded.all()[0].active);
    CHECK_EQ(reloaded.all()[2].percent, Percent(1));
    CHECK(!reloaded.all()[2].active);
    CHECK_EQ(reloaded.serialize(), store.serialize());

    // 4. Bad rows skipped with warnings, valid rows preserved
    {
        std::ofstream out(badFile, std::ios::binary);
        out << "2026-11-20 2026-11-30 10.00 true #\n";   // valid
        out << "2026-11-20 2026-11-30 10.00 true\n";     // missing #
        out << "2026-11-20 2026-11-30 10.00 #\n";        // wrong field count
        out << "2026-11-30 2026-11-20 10.00 true #\n";   // start > end
        out << "2026-11-20 2026-11-30 0.00 true #\n";    // 0%
        out << "2026-11-20 2026-11-30 150.00 true #\n";  // > 100%
        out << "2026-11-20 2026-11-30 10.00 maybe #\n";  // invalid bool
        out << "2025-02-29 2025-03-02 10.00 true #\n";   // invalid leap day
        out << "2026-12-01 2026-12-31 12.50 false #\n";  // valid
    }

    SeasonStore badStore;
    OperationResult badRes = badStore.load(badFile.string());
    CHECK(badRes.success);
    CHECK_EQ(badStore.all().size(), static_cast<std::size_t>(2));
    CHECK_EQ(badStore.warnings().size(), static_cast<std::size_t>(7));
    CHECK_EQ(badStore.all()[1].percent, Percent(1250));
    CHECK(!badStore.all()[1].active);

    removeTempFile(seasonFile);
    removeTempFile(badFile);
    return testSummary("test_season_store");
}
