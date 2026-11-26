#pragma once
// OWNER: Member 5. The six manager report classes (no Qt).
#include "Report.h"

class IncompleteOrdersReport : public IReport {
public:
    std::string name() const override;
    bool usesDateRange() const override;
    ReportTable build(const ReportData& data, const DateRange& range) const override;
};

class CompletedOrdersReport : public IReport {
public:
    std::string name() const override;
    bool usesDateRange() const override;
    ReportTable build(const ReportData& data, const DateRange& range) const override;
};

class CashSalesReport : public IReport {
public:
    std::string name() const override;
    bool usesDateRange() const override;
    ReportTable build(const ReportData& data, const DateRange& range) const override;
};

class AllSalesReport : public IReport {
public:
    std::string name() const override;
    bool usesDateRange() const override;
    ReportTable build(const ReportData& data, const DateRange& range) const override;
};

class Top10SoldReport : public IReport {
public:
    std::string name() const override;
    bool usesDateRange() const override;
    ReportTable build(const ReportData& data, const DateRange& range) const override;
};

class Top10InStockReport : public IReport {
public:
    std::string name() const override;
    bool usesDateRange() const override;
    ReportTable build(const ReportData& data, const DateRange& range) const override;
};
