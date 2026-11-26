#pragma once
#include <string>

struct Category {
    std::string id;        // 2 digits, 10-99
    std::string name;      // unique
    std::string acronym;   // max 8 characters
};