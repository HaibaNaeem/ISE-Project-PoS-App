#pragma once
// OWNER: Member 1. Reads and checks the 4-digit manager PIN from Settings.txt (no Qt).
#include <string>
#include "CommonTypes.h"

class ManagerAuth {
public:
    // Reads `manager_pin <4 digits> #` from path.
    // If the file is missing or the row is invalid, defaults to "1234" with a warning message.
    OperationResult load(const std::string& path);

    bool check(const std::string& pin) const;

private:
    std::string pin_ = "1234";
};
