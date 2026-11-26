// OWNER: Member 5. Unit tests for DateRange, ReportRegistry, and all six reports.
#include "DateRange.h"
#include "ReportRegistry.h"
#include "Reports.h"
#include "TestHelpers.h"

#include <memory>
#include <string>
#include <vector>

namespace {

Product makeProduct(const std::string& id,
                    const std::string& barcode,
                    const std::string& catId,
                    const std::string& acronym,
                    const std::string& name,
                    MeasureUnit unit,
                    Money price,
                    Money tax,
                    Money disc,
                    Quantity stock) {
    Product p;
    p.id = id;
    p.barcode = barcode;
    p.categoryId = catId;
    p.acronym = acronym;
    p.name = name;
    p.unit = unit;
    p.unitPrice = price;
    p.taxPerUnit = tax;
    p.discountPerUnit = disc;
    p.available = stock;
    return p;
}

class DummyCustomReport : public IReport {
public:
    std::string name() const override { return "Custom test"; }
    bool usesDateRange() const override { return false; }
    ReportTable build(const ReportData&, const DateRange&) const override {
        return {"Custom test", {"Col"}, {{"Val"}}, {}};
    }
};

} // namespace

int main() {
    // 1. DateRange presets
    {
        DateRange rToday = rangeFor(Period::Today, "2026-11-26");
        CHECK_EQ(rToday.from, std::string("2026-11-26"));
        CHECK_EQ(rToday.to, std::string("2026-11-26"));

        DateRange r7 = rangeFor(Period::Last7Days, "2026-11-26");
        CHECK_EQ(r7.from, std::string("2026-11-20"));
        CHECK_EQ(r7.to, std::string("2026-11-26"));

        DateRange r7CrossMonth = rangeFor(Period::Last7Days, "2026-03-02");
        CHECK_EQ(r7CrossMonth.from, std::string("2026-02-24"));
        CHECK_EQ(r7CrossMonth.to, std::string("2026-03-02"));

        DateRange rMonth = rangeFor(Period::ThisMonth, "2026-11-26");
        CHECK_EQ(rMonth.from, std::string("2026-11-01"));
        CHECK_EQ(rMonth.to, std::string("2026-11-26"));

        DateRange rYear = rangeFor(Period::ThisYear, "2026-11-26");
        CHECK_EQ(rYear.from, std::string("2026-01-01"));
        CHECK_EQ(rYear.to, std::string("2026-11-26"));

        CHECK(inDateRange("2026-11-20", r7));
        CHECK(inDateRange("2026-11-26", r7));
        CHECK(!inDateRange("2026-11-19", r7));
        CHECK(!inDateRange("2026-11-27", r7));
    }

    // 2. Build rich in-memory catalog (12 products to test Top 10 truncation and ties)
    std::vector<Product> products = {
        makeProduct("101", "120001", "12", "MILK", "Milk", MeasureUnit::Count, 45000, 2000, 1000, 10000),
        makeProduct("102", "100001", "10", "RICE", "Rice", MeasureUnit::Count, 100000, 0, 0, 50000),
        makeProduct("103", "200001", "20", "SUGAR", "Sugar", MeasureUnit::Weight, 16000, 0, 0, 25500),
        makeProduct("104", "300001", "30", "OIL", "Oil", MeasureUnit::Count, 50000, 0, 0, 30000),
        makeProduct("105", "400001", "40", "TEA", "Tea", MeasureUnit::Count, 38000, 0, 0, 30000), // tie with OIL in stock (30000)
        makeProduct("106", "500001", "50", "BISC", "Biscuit", MeasureUnit::Count, 6000, 0, 0, 100000),
        makeProduct("107", "600001", "60", "FLOUR", "Flour", MeasureUnit::Weight, 13500, 0, 0, 40000),
        makeProduct("108", "700001", "70", "SOAP", "Soap", MeasureUnit::Count, 11000, 0, 0, 35000),
        makeProduct("109", "800001", "80", "WATER", "Water", MeasureUnit::Count, 9000, 0, 0, 48000),
        makeProduct("110", "900001", "90", "JUICE", "Juice", MeasureUnit::Count, 15000, 0, 0, 20000),
        makeProduct("111", "910001", "91", "SALT", "Salt", MeasureUnit::Count, 5000, 0, 0, 15000),
        makeProduct("112", "920001", "92", "CHIPS", "Chips", MeasureUnit::Count, 8000, 0, 0, 5000)
    };
    ProductCatalog catalog(products);

    // Orders: 1 in-progress, 1 on-hold, 2 paid (1 cash, 1 card), 1 archived
    std::vector<Order> orders;
    {
        Order o;
        o.id = "00000126"; o.date = "2026-11-26"; o.customerName = "";
        o.status = OrderStatus::InProgress;
        o.lines = {{"101", "120001", 2000}}; // 2*(450+20-10) = 920.00 PKR
        orders.push_back(o);
    }
    {
        Order o;
        o.id = "00000226"; o.date = "2026-11-26"; o.customerName = "Sara";
        o.status = OrderStatus::OnHold;
        o.lines = {{"102", "100001", 1000}}; // 1000.00 PKR
        orders.push_back(o);
    }
    {
        Order o;
        o.id = "00000322"; o.date = "2026-11-22"; o.customerName = "Ali";
        o.status = OrderStatus::Paid; o.mode = PaymentMode::Cash; o.paymentAmount = 82800;
        orders.push_back(o);
    }
    {
        Order o;
        o.id = "00000426"; o.date = "2026-11-26"; o.customerName = "Bilal";
        o.status = OrderStatus::Paid; o.mode = PaymentMode::Card; o.paymentAmount = 100000;
        orders.push_back(o);
    }
    {
        Order o;
        o.id = "00000526"; o.date = "2026-11-26"; o.customerName = "Cancelled";
        o.status = OrderStatus::Archived; o.paymentAmount = 50000;
        orders.push_back(o);
    }

    // Bills: 1 cash bill in period (2026-11-22), 1 card bill in period (2026-11-26), 1 old cash bill outside period (2026-10-01)
    std::vector<Bill> bills;
    {
        Bill b;
        b.id = "000000001"; b.orderId = "00000322"; b.customerName = "Ali";
        b.date = "2026-11-22"; b.status = OrderStatus::Paid;
        b.itemDiscount = 2000; b.seasonalDiscount = 9200; b.specialDiscount = 0;
        b.payable = 82800;
        b.lines = {
            {"MILK", 45000, 2000, 2000},
            {"JUICE", 15000, 3000, 0},
            {"OIL", 50000, 3000, 0} // OIL = 3000, JUICE = 3000 -> tie!
        };
        bills.push_back(b);
    }
    {
        Bill b;
        b.id = "000000002"; b.orderId = "00000426"; b.customerName = "Bilal";
        b.date = "2026-11-26"; b.status = OrderStatus::Paid;
        // Note: simulate BillStore::load() where b.paymentMode defaults to Cash, so CashSales must check orders!
        b.paymentMode = PaymentMode::Cash;
        b.itemDiscount = 1000; b.seasonalDiscount = 5000; b.specialDiscount = 4000;
        b.payable = 100000;
        b.lines = {
            {"RICE", 100000, 1000, 1000},
            {"TEA", 38000, 4000, 0},
            {"BISC", 6000, 1000, 0},
            {"FLOUR", 13500, 1000, 0},
            {"SOAP", 11000, 1000, 0},
            {"WATER", 9000, 1000, 0},
            {"SALT", 5000, 1000, 0},
            {"CHIPS", 8000, 1000, 0},
            {"SUGAR", 16000, 500, 0} // 11 distinct products sold in Nov 20-26!
        };
        bills.push_back(b);
    }
    {
        Bill b;
        b.id = "000000003"; b.orderId = "00000001"; b.customerName = "Old";
        b.date = "2026-10-01"; b.status = OrderStatus::Paid;
        b.paymentMode = PaymentMode::Cash;
        b.itemDiscount = 0; b.seasonalDiscount = 0; b.specialDiscount = 0;
        b.payable = 50000;
        b.lines = {{"CHIPS", 8000, 50000, 0}};
        bills.push_back(b);
    }

    ReportData data{catalog, orders, bills};
    DateRange novRange{"2026-11-20", "2026-11-26"};

    // 3. ReportRegistry
    ReportRegistry registry = createDefaultReportRegistry();
    CHECK_EQ(registry.all().size(), static_cast<std::size_t>(6));
    registry.add(std::make_unique<DummyCustomReport>());
    CHECK_EQ(registry.all().size(), static_cast<std::size_t>(7));

    // 4. Incomplete orders
    {
        IncompleteOrdersReport rep;
        CHECK(!rep.usesDateRange());
        ReportTable t = rep.build(data, novRange);
        CHECK_EQ(t.rows.size(), static_cast<std::size_t>(2));
        CHECK_EQ(t.rows[0][0], std::string("00000126"));
        CHECK_EQ(t.rows[0][2], std::string("Walk-in"));
        CHECK_EQ(t.rows[0][3], std::string("in-progress"));
        CHECK_EQ(t.rows[0][4], std::string("920.00"));
        CHECK_EQ(t.rows[1][0], std::string("00000226"));
        CHECK_EQ(t.rows[1][4], std::string("1000.00"));
        CHECK_EQ(t.totalsRow[1], std::string("2"));
        CHECK_EQ(t.totalsRow[4], std::string("1920.00"));
    }

    // 5. Completed orders (archived excluded)
    {
        CompletedOrdersReport rep;
        CHECK(!rep.usesDateRange());
        ReportTable t = rep.build(data, novRange);
        CHECK_EQ(t.rows.size(), static_cast<std::size_t>(2));
        CHECK_EQ(t.rows[0][0], std::string("00000322"));
        CHECK_EQ(t.rows[0][3], std::string("cash"));
        CHECK_EQ(t.rows[1][0], std::string("00000426"));
        CHECK_EQ(t.rows[1][3], std::string("card"));
        CHECK_EQ(t.totalsRow[1], std::string("2"));
        CHECK_EQ(t.totalsRow[4], std::string("1828.00"));
    }

    // 6. Cash sales (only paid cash in period; card order 00000426 and Oct bill excluded)
    {
        CashSalesReport rep;
        CHECK(rep.usesDateRange());
        ReportTable t = rep.build(data, novRange);
        CHECK_EQ(t.rows.size(), static_cast<std::size_t>(1));
        CHECK_EQ(t.rows[0][0], std::string("000000001"));
        CHECK_EQ(t.rows[0][3], std::string("828.00"));
        CHECK_EQ(t.totalsRow[3], std::string("828.00"));
    }

    // 7. All sales (both Nov bills included, Oct excluded; discounts summed)
    {
        AllSalesReport rep;
        CHECK(rep.usesDateRange());
        ReportTable t = rep.build(data, novRange);
        CHECK_EQ(t.rows.size(), static_cast<std::size_t>(2));
        CHECK_EQ(t.rows[0][3], std::string("828.00"));
        CHECK_EQ(t.rows[0][4], std::string("112.00")); // 20 + 92 + 0
        CHECK_EQ(t.rows[1][3], std::string("1000.00"));
        CHECK_EQ(t.rows[1][4], std::string("100.00")); // 10 + 50 + 40
        CHECK_EQ(t.totalsRow[3], std::string("1828.00"));
        CHECK_EQ(t.totalsRow[4], std::string("212.00"));
    }

    // 8. Top 10 sold (11 products in Nov -> capped at 10; Oct bill ignored; ties by acronym)
    {
        Top10SoldReport rep;
        CHECK(rep.usesDateRange());
        ReportTable t = rep.build(data, novRange);
        CHECK_EQ(t.rows.size(), static_cast<std::size_t>(10));
        CHECK(t.totalsRow.empty());
        // #1: TEA (4), #2: JUICE (3), #3: OIL (3) -> JUICE < OIL alphabetically!
        CHECK_EQ(t.rows[0][0], std::string("TEA"));
        CHECK_EQ(t.rows[0][2], std::string("4"));
        CHECK_EQ(t.rows[1][0], std::string("JUICE"));
        CHECK_EQ(t.rows[2][0], std::string("OIL"));
        // #4: MILK (2)
        CHECK_EQ(t.rows[3][0], std::string("MILK"));
        // #5..#10 are 1 each (BISC, CHIPS, FLOUR, RICE, SALT, SOAP) and SUGAR (0.500) is 11th so dropped!
        CHECK_EQ(t.rows[4][0], std::string("BISC"));
        CHECK_EQ(t.rows[9][0], std::string("SOAP"));
    }

    // 9. Top 10 in stock (12 products -> top 10 descending; ties OIL & TEA at 30000 -> OIL < TEA)
    {
        Top10InStockReport rep;
        CHECK(!rep.usesDateRange());
        ReportTable t = rep.build(data, novRange);
        CHECK_EQ(t.rows.size(), static_cast<std::size_t>(10));
        CHECK(t.totalsRow.empty());
        CHECK_EQ(t.rows[0][0], std::string("BISC"));   // 100
        CHECK_EQ(t.rows[1][0], std::string("RICE"));   // 50
        CHECK_EQ(t.rows[2][0], std::string("WATER"));  // 48
        CHECK_EQ(t.rows[3][0], std::string("FLOUR"));  // 40.000
        CHECK_EQ(t.rows[4][0], std::string("SOAP"));   // 35
        CHECK_EQ(t.rows[5][0], std::string("OIL"));    // 30 (tie with TEA -> OIL first)
        CHECK_EQ(t.rows[6][0], std::string("TEA"));    // 30
        CHECK_EQ(t.rows[9][0], std::string("SALT"));   // 15 (MILK=10, CHIPS=5 left out)
    }

    return testSummary("test_reports");
}
