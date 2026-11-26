// OWNER: Member 5. Registry of IReport implementations (no Qt).
#include "ReportRegistry.h"
#include "Reports.h"

void ReportRegistry::add(std::unique_ptr<IReport> report) {
    if (report) {
        reports_.push_back(std::move(report));
    }
}

const std::vector<std::unique_ptr<IReport>>& ReportRegistry::all() const {
    return reports_;
}

ReportRegistry createDefaultReportRegistry() {
    ReportRegistry registry;
    registry.add(std::make_unique<IncompleteOrdersReport>());
    registry.add(std::make_unique<CompletedOrdersReport>());
    registry.add(std::make_unique<CashSalesReport>());
    registry.add(std::make_unique<AllSalesReport>());
    registry.add(std::make_unique<Top10SoldReport>());
    registry.add(std::make_unique<Top10InStockReport>());
    return registry;
}
