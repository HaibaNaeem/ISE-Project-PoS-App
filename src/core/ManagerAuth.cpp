// OWNER: Member 1. Reads and checks the 4-digit manager PIN from Settings.txt (no Qt).
#include "ManagerAuth.h"
#include "RecordFormat.h"
#include <cctype>
#include <vector>

namespace {

bool isFourDigits(const std::string& s) {
    if (s.size() != 4) return false;
    for (unsigned char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

std::string joinWarnings(const std::vector<std::string>& warnings) {
    std::string out;
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i > 0) out += "; ";
        out += warnings[i];
    }
    return out;
}

} // namespace

OperationResult ManagerAuth::load(const std::string& path) {
    pin_ = "1234";

    std::vector<std::string> lines;
    OperationResult readRes = readLines(path, lines);
    if (!readRes.success) {
        return {true, "Warning: settings file missing (" + path + "); using default PIN 1234."};
    }

    std::vector<std::string> warnings;
    bool foundPin = false;

    for (size_t idx = 0; idx < lines.size(); ++idx) {
        const int lineNo = static_cast<int>(idx) + 1;
        std::vector<std::string> fields;
        OperationResult splitRes = splitRecord(lines[idx], lineNo, fields);
        if (!splitRes.success) {
            warnings.push_back(splitRes.message + " (row skipped)");
            continue;
        }
        if (fields.empty()) {
            warnings.push_back("line " + std::to_string(lineNo) + ": empty record (row skipped)");
            continue;
        }
        if (fields[0] == "manager_pin") {
            if (fields.size() != 2 || !isFourDigits(fields[1])) {
                warnings.push_back("line " + std::to_string(lineNo) +
                                   ": invalid manager_pin (expected 4 digits)");
                continue;
            }
            pin_ = fields[1];
            foundPin = true;
        } else {
            warnings.push_back("line " + std::to_string(lineNo) +
                               ": unknown setting '" + fields[0] + "' (row skipped)");
        }
    }

    if (!foundPin) {
        pin_ = "1234";
        std::string msg = "Warning: no valid manager_pin in settings";
        if (!warnings.empty()) {
            msg += " (" + joinWarnings(warnings) + ")";
        }
        msg += "; using default PIN 1234.";
        return {true, msg};
    }

    if (!warnings.empty()) {
        return {true, "Manager PIN loaded with warning: " + joinWarnings(warnings)};
    }
    return {true, "Manager PIN loaded."};
}

bool ManagerAuth::check(const std::string& pin) const {
    return pin == pin_;
}
