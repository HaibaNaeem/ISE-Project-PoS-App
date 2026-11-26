#pragma once
// OWNER: Member 5. Core report interface and table data structures (no Qt).
#include <string>
#include <vector>
#include "Bill.h"
#include "DateRange.h"
#include "Order.h"
#include "ProductCatalog.h"

struct ReportTable {
    std::string title;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> totalsRow; // empty when report has no totals row
};

struct ReportData {
    const ProductCatalog& catalog;
    const std::vector<Order>& orders;
    const std::vector<Bill>& bills;
};

class IReport {
public:
    virtual ~IReport() = default;
    virtual std::string name() const = 0;
    virtual bool usesDateRange() const = 0;
    virtual ReportTable build(const ReportData& data, const DateRange& range) const = 0;
};
