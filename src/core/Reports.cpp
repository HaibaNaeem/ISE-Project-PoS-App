// OWNER: Member 5. The six manager report implementations (read-only, no Qt).
#include "Reports.h"
#include "Pricing.h"
#include "Units.h"
#include <algorithm>
#include <map>
#include <unordered_map>

namespace {

std::string displayCustomer(const std::string& name) {
    return name.empty() ? "Walk-in" : name;
}

std::string formatSoldQty(Quantity qty, const Product* product) {
    if (product) {
        return formatQuantity(qty, product->unit);
    }
    if (qty % 1000 == 0) {
        return formatQuantity(qty, MeasureUnit::Count);
    }
    return formatQuantity(qty, MeasureUnit::Weight);
}

} // namespace

// -----------------------------------------------------------------------------
// 1. Incomplete orders (in-progress and on-hold, current prices)
// -----------------------------------------------------------------------------
std::string IncompleteOrdersReport::name() const { return "Incomplete orders"; }
bool IncompleteOrdersReport::usesDateRange() const { return false; }

ReportTable IncompleteOrdersReport::build(const ReportData& data, const DateRange&) const {
    ReportTable table;
    table.title = name();
    table.headers = {"Order ID", "Date", "Customer", "Status", "Amount (PKR)"};

    std::size_t count = 0;
    Money totalAmount = 0;

    for (const Order& order : data.orders) {
        if (order.status != OrderStatus::InProgress && order.status != OrderStatus::OnHold) {
            continue;
        }
        const Bill draft = buildBill(order, data.catalog, "");
        const Money amount = draft.payable;
        table.rows.push_back({
            order.id,
            order.date,
            displayCustomer(order.customerName),
            orderStatusToText(order.status),
            formatMoney(amount)
        });
        ++count;
        totalAmount += amount;
    }

    table.totalsRow = {"Total", std::to_string(count), "", "", formatMoney(totalAmount)};
    return table;
}

// -----------------------------------------------------------------------------
// 2. Completed orders (paid orders only; archived is left out)
// -----------------------------------------------------------------------------
std::string CompletedOrdersReport::name() const { return "Completed orders"; }
bool CompletedOrdersReport::usesDateRange() const { return false; }

ReportTable CompletedOrdersReport::build(const ReportData& data, const DateRange&) const {
    ReportTable table;
    table.title = name();
    table.headers = {"Order ID", "Date", "Customer", "Payment Mode", "Amount (PKR)"};

    std::size_t count = 0;
    Money totalAmount = 0;

    for (const Order& order : data.orders) {
        if (order.status != OrderStatus::Paid) {
            continue;
        }
        table.rows.push_back({
            order.id,
            order.date,
            displayCustomer(order.customerName),
            paymentModeToText(order.mode),
            formatMoney(order.paymentAmount)
        });
        ++count;
        totalAmount += order.paymentAmount;
    }

    table.totalsRow = {"Total", std::to_string(count), "", "", formatMoney(totalAmount)};
    return table;
}

// -----------------------------------------------------------------------------
// 3. Cash sales (paid cash orders in the period)
// -----------------------------------------------------------------------------
std::string CashSalesReport::name() const { return "Cash sales"; }
bool CashSalesReport::usesDateRange() const { return true; }

ReportTable CashSalesReport::build(const ReportData& data, const DateRange& range) const {
    ReportTable table;
    table.title = name();
    table.headers = {"Bill ID", "Date", "Order ID", "Payable (PKR)"};

    std::unordered_map<std::string, const Order*> orderById;
    for (const Order& order : data.orders) {
        orderById[order.id] = &order;
    }

    Money totalPayable = 0;

    for (const Bill& bill : data.bills) {
        if (bill.status != OrderStatus::Paid || !inDateRange(bill.date, range)) {
            continue;
        }

        bool isCashPaid = false;
        const auto it = orderById.find(bill.orderId);
        if (it != orderById.end()) {
            const Order* order = it->second;
            isCashPaid = (order->status == OrderStatus::Paid && order->mode == PaymentMode::Cash);
        } else {
            isCashPaid = (bill.paymentMode == PaymentMode::Cash);
        }

        if (!isCashPaid) {
            continue;
        }

        table.rows.push_back({
            bill.id,
            bill.date,
            bill.orderId,
            formatMoney(bill.payable)
        });
        totalPayable += bill.payable;
    }

    table.totalsRow = {"Total", "", "", formatMoney(totalPayable)};
    return table;
}

// -----------------------------------------------------------------------------
// 4. All sales (every bill in the period)
// -----------------------------------------------------------------------------
std::string AllSalesReport::name() const { return "All sales"; }
bool AllSalesReport::usesDateRange() const { return true; }

ReportTable AllSalesReport::build(const ReportData& data, const DateRange& range) const {
    ReportTable table;
    table.title = name();
    table.headers = {"Bill ID", "Date", "Customer", "Payable (PKR)", "Discounts (PKR)"};

    Money totalPayable = 0;
    Money totalDiscounts = 0;

    for (const Bill& bill : data.bills) {
        if (!inDateRange(bill.date, range)) {
            continue;
        }
        const Money combinedDiscount = bill.itemDiscount + bill.seasonalDiscount + bill.specialDiscount;
        table.rows.push_back({
            bill.id,
            bill.date,
            displayCustomer(bill.customerName),
            formatMoney(bill.payable),
            formatMoney(combinedDiscount)
        });
        totalPayable += bill.payable;
        totalDiscounts += combinedDiscount;
    }

    table.totalsRow = {"Total", "", "", formatMoney(totalPayable), formatMoney(totalDiscounts)};
    return table;
}

// -----------------------------------------------------------------------------
// 5. Top 10 sold (most sold first in period; ties by acronym)
// -----------------------------------------------------------------------------
std::string Top10SoldReport::name() const { return "Top 10 sold"; }
bool Top10SoldReport::usesDateRange() const { return true; }

ReportTable Top10SoldReport::build(const ReportData& data, const DateRange& range) const {
    ReportTable table;
    table.title = name();
    table.headers = {"Acronym", "Name", "Quantity Sold", "Remaining Stock"};

    std::unordered_map<std::string, const Product*> productByAcronym;
    for (const Product& p : data.catalog.all()) {
        productByAcronym[p.acronym] = &p;
    }

    std::map<std::string, Quantity> soldByAcronym;
    for (const Bill& bill : data.bills) {
        if (bill.status != OrderStatus::Paid || !inDateRange(bill.date, range)) {
            continue;
        }
        for (const BillLine& line : bill.lines) {
            if (line.quantity > 0) {
                soldByAcronym[line.acronym] += line.quantity;
            }
        }
    }

    struct SoldEntry {
        std::string acronym;
        Quantity sold = 0;
    };
    std::vector<SoldEntry> entries;
    entries.reserve(soldByAcronym.size());
    for (const auto& [acronym, qty] : soldByAcronym) {
        if (qty > 0) {
            entries.push_back({acronym, qty});
        }
    }

    std::sort(entries.begin(), entries.end(), [](const SoldEntry& a, const SoldEntry& b) {
        if (a.sold != b.sold) return a.sold > b.sold;
        return a.acronym < b.acronym;
    });

    if (entries.size() > 10) {
        entries.resize(10);
    }

    for (const SoldEntry& e : entries) {
        const auto it = productByAcronym.find(e.acronym);
        const Product* prod = (it != productByAcronym.end()) ? it->second : nullptr;
        const std::string prodName = prod ? prod->name : e.acronym;
        const std::string remaining = prod ? formatQuantity(prod->available, prod->unit) : "0";
        table.rows.push_back({
            e.acronym,
            prodName,
            formatSoldQty(e.sold, prod),
            remaining
        });
    }

    return table;
}

// -----------------------------------------------------------------------------
// 6. Top 10 in stock (most stock first; ties by acronym)
// -----------------------------------------------------------------------------
std::string Top10InStockReport::name() const { return "Top 10 in stock"; }
bool Top10InStockReport::usesDateRange() const { return false; }

ReportTable Top10InStockReport::build(const ReportData& data, const DateRange&) const {
    ReportTable table;
    table.title = name();
    table.headers = {"Acronym", "Name", "Remaining Stock"};

    std::vector<Product> products = data.catalog.all();
    std::sort(products.begin(), products.end(), [](const Product& a, const Product& b) {
        if (a.available != b.available) return a.available > b.available;
        return a.acronym < b.acronym;
    });

    if (products.size() > 10) {
        products.resize(10);
    }

    for (const Product& p : products) {
        table.rows.push_back({
            p.acronym,
            p.name,
            formatQuantity(p.available, p.unit)
        });
    }

    return table;
}
