// Tests for Member 4 Pricing module (Increment 3)
#include "TestHelpers.h"
#include "Pricing.h"
#include "DiscountRules.h"
#include "Product.h"
#include "ProductCatalog.h"
#include "Order.h"

// Mandatory check from spec:
// price 100.00, tax 15.00, discount 10.00, qty 3
// Paisa: 10000, 1500, 1000, qty 3000
// Expected: subtotal 30000, tax 4500, discount 3000 -> base 31500
// seasonal 10% = 3150, special 8% = 2520, payable = 25830 (258.30 PKR)
static void testMandatory300_45_30Case() {
    Product p;
    p.id = "101";
    p.acronym = "ITEMA";
    p.name = "Item A";
    p.unitPrice = 10000;
    p.taxPerUnit = 1500;
    p.discountPerUnit = 1000;
    p.available = 10000;

    ProductCatalog catalog({p});

    Order order;
    order.id = "00000001";
    order.customerName = "Ali";
    order.date = "2026-11-25";
    order.time = "10:00:00";
    order.mode = PaymentMode::Cash;
    order.lines = { {"101", "", 3000} };

    BillOptions opts;
    opts.billId = "000000001";
    opts.date = "2026-11-25";
    opts.time = "10:00:00";
    opts.seasonalPercent = 1000; // 10.00%
    opts.specialPercent = 800;   // 8.00%
    opts.received = 30000;       // 300.00 PKR tendered

    Bill bill = buildBill(order, catalog, opts);

    CHECK_EQ(bill.subtotal, 30000);
    CHECK_EQ(bill.tax, 4500);
    CHECK_EQ(bill.itemDiscount, 3000);
    CHECK_EQ(bill.seasonalDiscount, 3150);
    CHECK_EQ(bill.specialDiscount, 2520);
    CHECK_EQ(bill.payable, 25830);
    CHECK_EQ(bill.received, 30000);
    CHECK_EQ(bill.balance, 4170); // 300.00 - 258.30 = 41.70 (4170 paisa)
    CHECK_EQ(bill.lines.size(), 1);
    CHECK_EQ(bill.lines[0].acronym, std::string("ITEMA"));
    CHECK_EQ(bill.lines[0].unitPrice, 10000);
    CHECK_EQ(bill.lines[0].quantity, 3000);
    CHECK_EQ(bill.lines[0].discount, 3000);
}

// File example check from team plan:
// 2 x 450.00, tax 20.00/unit, discount 10.00/unit, seasonal 10%
// Expected: 900.00 / 40.00 / 20.00, base 920.00, seasonal 92.00, payable 828.00, received 1000.00, balance 172.00
static void testFileExampleMilk() {
    Product milk;
    milk.id = "101";
    milk.acronym = "MILKP";
    milk.unitPrice = 45000;      // 450.00
    milk.taxPerUnit = 2000;      // 20.00
    milk.discountPerUnit = 1000; // 10.00
    milk.available = 10000;

    ProductCatalog catalog({milk});

    Order order;
    order.id = "00000722";
    order.customerName = "Ali Khan";
    order.date = "2026-11-22";
    order.time = "14:05:09";
    order.mode = PaymentMode::Cash;
    order.lines = { {"101", "", 2000} }; // 2 pcs

    BillOptions opts;
    opts.billId = "000000031";
    opts.date = "2026-11-22";
    opts.time = "14:05:09";
    opts.seasonalPercent = 1000; // 10.00%
    opts.specialPercent = 0;
    opts.received = 100000;      // 1000.00 PKR tendered

    Bill bill = buildBill(order, catalog, opts);

    CHECK_EQ(bill.id, std::string("000000031"));
    CHECK_EQ(bill.customerName, std::string("Ali Khan"));
    CHECK_EQ(bill.orderId, std::string("00000722"));
    CHECK_EQ(bill.subtotal, 90000);
    CHECK_EQ(bill.tax, 4000);
    CHECK_EQ(bill.itemDiscount, 2000);
    CHECK_EQ(bill.seasonalDiscount, 9200);
    CHECK_EQ(bill.specialDiscount, 0);
    CHECK_EQ(bill.payable, 82800);
    CHECK_EQ(bill.received, 100000);
    CHECK_EQ(bill.balance, 17200);
    CHECK_EQ(bill.lines.size(), 1);
    CHECK_EQ(bill.lines[0].acronym, std::string("MILKP"));
    CHECK_EQ(bill.lines[0].quantity, 2000);
    CHECK_EQ(bill.lines[0].discount, 2000);
}

// Seasonal only, Special only, Both
static void testDiscountVariations() {
    Product p;
    p.id = "1";
    p.acronym = "PROD";
    p.unitPrice = 10000; // 100.00
    p.taxPerUnit = 0;
    p.discountPerUnit = 0;
    ProductCatalog catalog({p});

    Order order;
    order.lines = { {"1", "", 1000} }; // 1 pc -> base = 10000 (100.00)

    // Seasonal only: 10%
    BillOptions opt1;
    opt1.seasonalPercent = 1000;
    opt1.specialPercent = 0;
    Bill b1 = buildBill(order, catalog, opt1);
    CHECK_EQ(b1.subtotal, 10000);
    CHECK_EQ(b1.seasonalDiscount, 1000); // 10.00
    CHECK_EQ(b1.specialDiscount, 0);
    CHECK_EQ(b1.payable, 9000);          // 90.00

    // Special only: 5% (500)
    BillOptions opt2;
    opt2.seasonalPercent = 0;
    opt2.specialPercent = 500;
    Bill b2 = buildBill(order, catalog, opt2);
    CHECK_EQ(b2.seasonalDiscount, 0);
    CHECK_EQ(b2.specialDiscount, 500);   // 5.00
    CHECK_EQ(b2.payable, 9500);          // 95.00

    // Both: 10% seasonal + 8% special
    BillOptions opt3;
    opt3.seasonalPercent = 1000;
    opt3.specialPercent = 800;
    Bill b3 = buildBill(order, catalog, opt3);
    CHECK_EQ(b3.seasonalDiscount, 1000); // 10.00
    CHECK_EQ(b3.specialDiscount, 800);   // 8.00
    CHECK_EQ(b3.payable, 8200);          // 82.00
}

// Special discount above 8.00% (801) is refused
static void testSpecialAbove8Refused() {
    SpecialDiscountRule rule;
    OperationResult valid800 = rule.validatePercent(800);
    CHECK(valid800.success);

    OperationResult invalid801 = rule.validatePercent(801);
    CHECK(!invalid801.success);
    CHECK(invalid801.message.find("8.00%") != std::string::npos ||
          invalid801.message.find("800") != std::string::npos);

    // Rule amount for > 800 returns 0
    BillOptions opts;
    opts.specialPercent = 801;
    Money amt = rule.amount(10000, opts);
    CHECK_EQ(amt, 0);

    // In buildBill: special discount is not applied if > 800
    Product p;
    p.id = "1";
    p.acronym = "P";
    p.unitPrice = 10000;
    ProductCatalog catalog({p});
    Order ord;
    ord.lines = { {"1", "", 1000} };

    Bill b = buildBill(ord, catalog, opts);
    CHECK_EQ(b.specialDiscount, 0);
    CHECK_EQ(b.payable, 10000);
}

// Zero quantity test
static void testZeroQuantity() {
    Product p;
    p.id = "101";
    p.acronym = "ITEM";
    p.unitPrice = 5000;
    p.taxPerUnit = 500;
    p.discountPerUnit = 200;

    BillLine line = priceLine(p, 0);
    CHECK_EQ(line.quantity, 0);
    CHECK_EQ(line.discount, 0);

    ProductCatalog catalog({p});
    Order order;
    order.lines = { {"101", "", 0} };

    Bill bill = buildBill(order, catalog, "BILL_ZERO");
    CHECK_EQ(bill.subtotal, 0);
    CHECK_EQ(bill.tax, 0);
    CHECK_EQ(bill.itemDiscount, 0);
    CHECK_EQ(bill.payable, 0);
}

// Rounding half up tests
static void testRoundingHalfUp() {
    // 1.500 kg at 33.33 PKR -> 3333 paisa
    // (3333 * 1500 + 500) / 1000 = (4999500 + 500) / 1000 = 5000 paisa
    Product p;
    p.id = "103";
    p.acronym = "WEIGHT";
    p.unit = MeasureUnit::Weight;
    p.unitPrice = 3333;
    p.taxPerUnit = 0;
    p.discountPerUnit = 0;

    BillLine line = priceLine(p, 1500);
    CHECK_EQ(line.discount, 0);

    ProductCatalog catalog({p});
    Order order;
    order.lines = { {"103", "", 1500} };

    Bill bill = buildBill(order, catalog, "B1");
    CHECK_EQ(bill.subtotal, 5000);
    CHECK_EQ(bill.payable, 5000);

    // Percent rounding: 33.33% of 100.00 PKR (10000 paisa)
    // 10000 * 3333 = 33330000 -> (33330000 + 5000) / 10000 = 3333
    CHECK_EQ(BillingMath::percentOf(10000, 3333), 3333);
}

// Several products on bill
static void testSeveralProducts() {
    Product p1{"1", "111111", "MILK", "Milk", MeasureUnit::Count, 45000, 2000, 1000, 10000, ""};
    Product p2{"2", "222222", "BREAD", "Bread", MeasureUnit::Count, 12000, 0, 500, 5000, ""};
    Product p3{"3", "333333", "EGG", "Eggs", MeasureUnit::Count, 3000, 100, 0, 30000, ""};

    ProductCatalog catalog({p1, p2, p3});

    Order order;
    order.id = "ORD01";
    order.customerName = "Bilal";
    order.lines = {
        {"1", "", 2000}, // 2 Milk: sub 900.00 (90000), tax 40.00 (4000), disc 20.00 (2000)
        {"2", "", 1000}, // 1 Bread: sub 120.00 (12000), tax 0, disc 5.00 (500)
        {"3", "", 6000}  // 6 Eggs: sub 18.00 (1800), tax 0.60 (60), disc 0
    };

    BillOptions opts;
    opts.billId = "BILL_MULTI";
    opts.seasonalPercent = 500; // 5%
    opts.specialPercent = 200;  // 2%

    Bill bill = buildBill(order, catalog, opts);

    CHECK_EQ(bill.lines.size(), 3);
    CHECK_EQ(bill.subtotal, 90000 + 12000 + 18000); // 120000
    CHECK_EQ(bill.tax, 4000 + 0 + 600);             // 4600
    CHECK_EQ(bill.itemDiscount, 2000 + 500 + 0);    // 2500

    Money base = 120000 + 4600 - 2500; // 122100
    // Seasonal 5% = percentOf(122100, 500) = (61050000 + 5000) / 10000 = 6105
    CHECK_EQ(bill.seasonalDiscount, 6105);
    // Special 2% = percentOf(122100, 200) = (24420000 + 5000) / 10000 = 2442
    CHECK_EQ(bill.specialDiscount, 2442);
    CHECK_EQ(bill.payable, base - 6105 - 2442); // 122100 - 8547 = 113553
}

// Cash change calculation
static void testChangeDue() {
    CHECK_EQ(changeDue(31500, 31500), 0);
    CHECK_EQ(changeDue(31500, 35000), 3500);
    CHECK_EQ(changeDue(31500, 30000), 0);
    CHECK_EQ(changeDue(0, 5000), 5000);
    CHECK_EQ(calculateChange(10000, 15000), 5000);
}

int main() {
    testMandatory300_45_30Case();
    testFileExampleMilk();
    testDiscountVariations();
    testSpecialAbove8Refused();
    testZeroQuantity();
    testRoundingHalfUp();
    testSeveralProducts();
    testChangeDue();
    return testSummary("test_pricing");
}
