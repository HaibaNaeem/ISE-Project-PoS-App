#pragma once

#include <string>
#include <vector>
#include "CommonTypes.h"

OperationResult splitRecord(
    const std::string& line,
    int lineNumber,
    std::vector<std::string>& fields
);

std::string quoteField(const std::string& text);

std::string writeRecord(const std::vector<std::string>& fields);

OperationResult readLines(
    const std::string& path,
    std::vector<std::string>& lines
);

class FileTransaction {
public:
    void add(const std::string& targetPath, const std::string& newContent);

    OperationResult commit();

    void simulateFailure(bool enable) {
        simulateFailure_ = enable;
    }

private:
    struct Entry {
        std::string target;
        std::string content;
    };

    std::vector<Entry> entries_;
    bool simulateFailure_ = false;
};