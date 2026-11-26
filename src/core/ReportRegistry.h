#pragma once
// OWNER: Member 5. Registry of IReport implementations (Open/Closed Principle, no Qt).
#include <memory>
#include <vector>
#include "Report.h"

class ReportRegistry {
public:
    void add(std::unique_ptr<IReport> report);
    const std::vector<std::unique_ptr<IReport>>& all() const;

private:
    std::vector<std::unique_ptr<IReport>> reports_;
};

// Registers all six standard manager reports into a new registry.
ReportRegistry createDefaultReportRegistry();
