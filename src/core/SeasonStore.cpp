// OWNER: Member 5. Persistence and validation for Seasons.txt (no Qt).
#include "SeasonStore.h"
#include "RecordFormat.h"
#include "Units.h"
#include <cctype>
#include <sstream>

namespace {

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && isLeapYear(year)) return 29;
    return kDays[month - 1];
}

bool isValidIsoDate(const std::string& s) {
    if (s.size() != 10) return false;
    for (size_t i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) {
            if (s[i] != '-') return false;
        } else {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        }
    }
    const int year = std::stoi(s.substr(0, 4));
    const int month = std::stoi(s.substr(5, 2));
    const int day = std::stoi(s.substr(8, 2));
    if (year < 1900 || month < 1 || month > 12) return false;
    const int maxDay = daysInMonth(year, month);
    return day >= 1 && day <= maxDay;
}

OperationResult validateSeason(const Season& s) {
    if (!isValidIsoDate(s.startDate)) {
        return {false, "Invalid start date '" + s.startDate + "' (expected YYYY-MM-DD)."};
    }
    if (!isValidIsoDate(s.endDate)) {
        return {false, "Invalid end date '" + s.endDate + "' (expected YYYY-MM-DD)."};
    }
    if (s.startDate > s.endDate) {
        return {false, "Start date must be on or before end date."};
    }
    if (s.percent <= 0 || s.percent > 10000) {
        return {false, "Season percent must be above 0.00 and at most 100.00."};
    }
    return {true, "Season valid."};
}

} // namespace

OperationResult SeasonStore::load(const std::string& path) {
    seasons_.clear();
    warnings_.clear();

    std::vector<std::string> lines;
    OperationResult r = readLines(path, lines);
    if (!r.success) return r;

    int lineNo = 0;
    for (const auto& line : lines) {
        ++lineNo;
        std::vector<std::string> fields;
        OperationResult splitRes = splitRecord(line, lineNo, fields);
        if (!splitRes.success) {
            warnings_.push_back(splitRes.message + " (row skipped)");
            continue;
        }
        if (fields.size() != 4) {
            warnings_.push_back("line " + std::to_string(lineNo) +
                                ": expected 4 fields (row skipped)");
            continue;
        }

        Season s;
        s.startDate = fields[0];
        s.endDate = fields[1];

        OperationResult pctRes = parsePercent(fields[2], s.percent);
        if (!pctRes.success) {
            warnings_.push_back("line " + std::to_string(lineNo) +
                                ": " + pctRes.message + " (row skipped)");
            continue;
        }

        if (fields[3] == "true") {
            s.active = true;
        } else if (fields[3] == "false") {
            s.active = false;
        } else {
            warnings_.push_back("line " + std::to_string(lineNo) +
                                ": active must be true or false (row skipped)");
            continue;
        }

        OperationResult valid = validateSeason(s);
        if (!valid.success) {
            warnings_.push_back("line " + std::to_string(lineNo) +
                                ": " + valid.message + " (row skipped)");
            continue;
        }

        seasons_.push_back(s);
    }

    OperationResult ok;
    ok.success = true;
    ok.message = std::to_string(seasons_.size()) + " seasons loaded";
    if (!warnings_.empty()) {
        ok.message += ", " + std::to_string(warnings_.size()) + " rows skipped";
    }
    return ok;
}

std::string SeasonStore::serialize() const {
    std::ostringstream out;
    for (const auto& s : seasons_) {
        out << writeRecord({
                   s.startDate,
                   s.endDate,
                   formatPercent(s.percent),
                   s.active ? "true" : "false"
               })
            << "\n";
    }
    return out.str();
}

OperationResult SeasonStore::save(const std::string& path) const {
    FileTransaction tx;
    tx.add(path, serialize());
    return tx.commit();
}

OperationResult SeasonStore::add(const Season& season) {
    OperationResult valid = validateSeason(season);
    if (!valid.success) return valid;
    seasons_.push_back(season);
    return {true, "Season added."};
}

OperationResult SeasonStore::setActive(std::size_t index, bool active) {
    if (index >= seasons_.size()) {
        return {false, "Season index out of range."};
    }
    seasons_[index].active = active;
    return {true, active ? "Season activated." : "Season deactivated."};
}
