// OWNER: Member 1. Tests for ManagerAuth (right PIN, wrong PIN, missing file, bad rows).
#include "ManagerAuth.h"
#include "TestHelpers.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path writeTempSettings(const std::string& name, const std::string& content) {
    fs::path p = fs::temp_directory_path() / ("pos_test_" + name + ".txt");
    std::ofstream out(p, std::ios::binary);
    out << content;
    return p;
}

} // namespace

int main() {
    // 1. Default before load()
    {
        ManagerAuth auth;
        CHECK(auth.check("1234"));
        CHECK(!auth.check("0000"));
        CHECK(!auth.check(""));
    }

    // 2. Missing file falls back to 1234 with a warning
    {
        ManagerAuth auth;
        fs::path missing = fs::temp_directory_path() / "pos_test_nonexistent_settings_999.txt";
        std::error_code ec;
        fs::remove(missing, ec);

        OperationResult r = auth.load(missing.string());
        CHECK(r.success);
        CHECK(!r.message.empty());
        CHECK(auth.check("1234"));
        CHECK(!auth.check("5678"));
    }

    // 3. Valid PIN in Settings.txt: right PIN accepted, wrong PINs rejected
    {
        fs::path valid = writeTempSettings("valid_pin", "manager_pin 5678 #\n");
        ManagerAuth auth;
        OperationResult r = auth.load(valid.string());
        CHECK(r.success);
        CHECK(auth.check("5678"));
        CHECK(!auth.check("1234"));
        CHECK(!auth.check("567"));
        CHECK(!auth.check("56789"));
        CHECK(!auth.check(""));
        CHECK(!auth.check("567a"));
        std::error_code ec;
        fs::remove(valid, ec);
    }

    // 4. Leading zeros preserved ("0042")
    {
        fs::path leadingZero = writeTempSettings("zero_pin", "manager_pin 0042 #\n");
        ManagerAuth auth;
        OperationResult r = auth.load(leadingZero.string());
        CHECK(r.success);
        CHECK(auth.check("0042"));
        CHECK(!auth.check("42"));
        std::error_code ec;
        fs::remove(leadingZero, ec);
    }

    // 5. Bad rows fall back to 1234 with warning (no crash)
    {
        const std::vector<std::string> badInputs = {
            "",
            "manager_pin 12ab #\n",
            "manager_pin 123 #\n",
            "manager_pin 12345 #\n",
            "manager_pin 9999\n",
            "manager_pin #\n",
            "other_setting 9999 #\n"
        };
        for (size_t i = 0; i < badInputs.size(); ++i) {
            fs::path p = writeTempSettings("bad_" + std::to_string(i), badInputs[i]);
            ManagerAuth auth;
            // First load a non-default PIN to ensure bad load resets to 1234
            fs::path reset = writeTempSettings("temp_valid", "manager_pin 9876 #\n");
            auth.load(reset.string());
            CHECK(auth.check("9876"));

            OperationResult r = auth.load(p.string());
            CHECK(r.success);
            CHECK(r.message.find("Warning") != std::string::npos);
            CHECK(auth.check("1234"));
            CHECK(!auth.check("9876"));

            std::error_code ec;
            fs::remove(p, ec);
            fs::remove(reset, ec);
        }
    }

    // 6. Bad row followed by valid row loads valid PIN with warning
    {
        fs::path mixed = writeTempSettings("mixed_pin",
                                           "bad_line_no_hash\n"
                                           "manager_pin 4321 #\n");
        ManagerAuth auth;
        OperationResult r = auth.load(mixed.string());
        CHECK(r.success);
        CHECK(auth.check("4321"));
        CHECK(!auth.check("1234"));
        std::error_code ec;
        fs::remove(mixed, ec);
    }

    return testSummary("test_manager_auth");
}
