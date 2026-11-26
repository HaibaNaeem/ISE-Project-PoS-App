// Tests for Member 4 CheckoutService (Increment 3)
#include "TestHelpers.h"
#include "CheckoutService.h"
#include "ProductCatalog.h"
#include "OrderStore.h"
#include "BillStore.h"
#include "OrderManager.h"
#include "Pricing.h"
#include "SeasonService.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

void cleanFolder(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

std::string readFileContent(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct TestOrderManagerAccessor {
    const ProductCatalog* catalog;
    OrderStore* store;
    std::optional<Order> current;
    std::function<DateTime()> clock;
};

void setTestActiveOrder(OrderManager& mgr, const Order& ord) {
    reinterpret_cast<TestOrderManagerAccessor*>(&mgr)->current = ord;
}

} // anonymous namespace

// 1. Mandatory check: Cash change 1000.00 for 828.00 gives 172.00
static void testCashPaymentChange1000For828() {
    fs::path tempDir = fs::temp_directory_path() / "test_checkout_cash_828";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path prodFile   = tempDir / "Product.txt";
    fs::path ordFile    = tempDir / "Order.txt";
    fs::path ordDetFile = tempDir / "OrderDetail.txt";
    fs::path billFile   = tempDir / "Bill.txt";
    fs::path billDetFile= tempDir / "BillDetail.txt";

    Product milk{"101", "123456", "MILKP", "Milk Powder", MeasureUnit::Count, 45000, 2000, 1000, 10000, ""};
    ProductCatalog catalog({milk});
    OrderStore orders(ordFile.string(), ordDetFile.string());
    BillStore bills(billFile.string(), billDetFile.string());

    Season novSeason{"2026-11-20", "2026-11-30", 1000, true}; // 10.00% active
    SeasonService seasons({novSeason});

    CheckoutService service(catalog, orders, bills, prodFile.string(), seasons);
    OrderManager manager(catalog, &orders);

    Order testOrder;
    testOrder.id = "00000722";
    testOrder.customerName = "Ali Khan";
    testOrder.date = "2026-11-22";
    testOrder.time = "14:05:09";
    testOrder.lines = { {"101", "123456", 2000} }; // 2 pcs
    setTestActiveOrder(manager, testOrder);

    PaymentInfo payment;
    payment.mode = PaymentMode::Cash;
    payment.received = 100000; // 1000.00 PKR tendered
    payment.specialPercent = 0;
    payment.specialApproved = false;

    Bill outBill;
    OperationResult r = service.pay(manager, payment, outBill);
    CHECK(r.success);
    CHECK_EQ(outBill.subtotal, 90000);
    CHECK_EQ(outBill.tax, 4000);
    CHECK_EQ(outBill.itemDiscount, 2000);
    CHECK_EQ(outBill.seasonalDiscount, 9200);
    CHECK_EQ(outBill.payable, 82800);
    CHECK_EQ(outBill.received, 100000);
    CHECK_EQ(outBill.balance, 17200); // 172.00 PKR

    // Catalog stock reduced from 10 to 8
    auto pAfter = catalog.findById("101");
    CHECK(pAfter.has_value());
    CHECK_EQ(pAfter->available, 8000);

    // Active order cleared
    CHECK(!manager.current().has_value());

    // Disk files created
    CHECK(fs::exists(prodFile));
    CHECK(fs::exists(ordFile));
    CHECK(fs::exists(ordDetFile));
    CHECK(fs::exists(billFile));
    CHECK(fs::exists(billDetFile));

    cleanFolder(tempDir);
}

// 2. Card without merchant is refused
static void testCardWithoutMerchantRefused() {
    fs::path tempDir = fs::temp_directory_path() / "test_checkout_card_no_merchant";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    Product p{"101", "111111", "ITEM", "Item", MeasureUnit::Count, 10000, 0, 0, 5000, ""};
    ProductCatalog catalog({p});
    OrderStore orders;
    BillStore bills;
    SeasonService seasons;
    CheckoutService service(catalog, orders, bills, "", seasons);
    OrderManager manager(catalog, &orders);

    Order ord;
    ord.id = "ORD01";
    ord.lines = { {"101", "", 1000} };
    setTestActiveOrder(manager, ord);

    PaymentInfo payment;
    payment.mode = PaymentMode::Card;
    payment.merchant = ""; // Missing merchant
    payment.received = 10000;

    Bill out;
    OperationResult r = service.pay(manager, payment, out);
    CHECK(!r.success);
    CHECK(r.message.find("Merchant") != std::string::npos || r.message.find("merchant") != std::string::npos);

    cleanFolder(tempDir);
}

// 3. Special discount without approval is refused, > 800 refused
static void testSpecialWithoutApprovalRefused() {
    fs::path tempDir = fs::temp_directory_path() / "test_checkout_special";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    Product p{"101", "111111", "ITEM", "Item", MeasureUnit::Count, 10000, 0, 0, 5000, ""};
    ProductCatalog catalog({p});
    OrderStore orders;
    BillStore bills;
    SeasonService seasons;
    CheckoutService service(catalog, orders, bills, "", seasons);
    OrderManager manager(catalog, &orders);

    Order ord;
    ord.id = "ORD01";
    ord.lines = { {"101", "", 1000} };
    setTestActiveOrder(manager, ord);

    // 5% special without approval
    PaymentInfo payment;
    payment.mode = PaymentMode::Cash;
    payment.specialPercent = 500;
    payment.specialApproved = false; // Not approved!
    payment.received = 10000;

    Bill out;
    OperationResult r1 = service.pay(manager, payment, out);
    CHECK(!r1.success);
    CHECK(r1.message.find("approval") != std::string::npos || r1.message.find("Approval") != std::string::npos);

    // 8.01% special with approval (exceeds 8.00% cap)
    payment.specialPercent = 801;
    payment.specialApproved = true;
    OperationResult r2 = service.pay(manager, payment, out);
    CHECK(!r2.success);
    CHECK(r2.message.find("8.00%") != std::string::npos || r2.message.find("800") != std::string::npos);

    cleanFolder(tempDir);
}

// 4. Forced write failure leaves all five files and stock unchanged (all-or-nothing rollback)
static void testForcedFailureRollback() {
    fs::path tempDir = fs::temp_directory_path() / "test_checkout_forced_fail";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path prodFile   = tempDir / "Product.txt";
    fs::path ordFile    = tempDir / "Order.txt";
    fs::path ordDetFile = tempDir / "OrderDetail.txt";
    fs::path billFile   = tempDir / "Bill.txt";
    fs::path billDetFile= tempDir / "BillDetail.txt";

    Product p{"101", "111111", "ITEM", "Item", MeasureUnit::Count, 10000, 0, 0, 10000, ""};
    ProductCatalog catalog({p});
    OrderStore orders(ordFile.string(), ordDetFile.string());
    BillStore bills(billFile.string(), billDetFile.string());

    // Save initial state to disk
    {
        std::ofstream(prodFile) << catalog.serialize();
        std::ofstream(ordFile) << "";
        std::ofstream(ordDetFile) << "";
        std::ofstream(billFile) << "";
        std::ofstream(billDetFile) << "";
    }
    std::string initialProdContent = readFileContent(prodFile.string());

    SeasonService seasons;
    CheckoutService service(catalog, orders, bills, prodFile.string(), seasons);
    OrderManager manager(catalog, &orders);

    Order ord;
    ord.id = "ORD01";
    ord.lines = { {"101", "", 2000} };
    setTestActiveOrder(manager, ord);

    PaymentInfo payment;
    payment.mode = PaymentMode::Cash;
    payment.received = 20000;

    // Simulate write failure
    service.simulateWriteFailure(true);

    Bill out;
    OperationResult r = service.pay(manager, payment, out);
    CHECK(!r.success);

    // In-memory catalog stock unchanged
    CHECK_EQ(catalog.findById("101")->available, 10000);
    // Bills in store unchanged
    CHECK_EQ(bills.bills().size(), 0);
    // Active order remains in manager
    CHECK(manager.current().has_value());
    // File on disk unchanged
    CHECK_EQ(readFileContent(prodFile.string()), initialProdContent);

    cleanFolder(tempDir);
}

// 5. saveEdited: stock 10 and quantity 2 to 5 leaves stock 7, same Bill ID and date
static void testSaveEditedStockDifference() {
    fs::path tempDir = fs::temp_directory_path() / "test_checkout_save_edited";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path prodFile   = tempDir / "Product.txt";
    fs::path ordFile    = tempDir / "Order.txt";
    fs::path ordDetFile = tempDir / "OrderDetail.txt";
    fs::path billFile   = tempDir / "Bill.txt";
    fs::path billDetFile= tempDir / "BillDetail.txt";

    // Available stock in catalog at time of editing is 10 pcs (10000)
    Product p{"101", "123456", "MILKP", "Milk Powder", MeasureUnit::Count, 45000, 2000, 1000, 10000, ""};
    ProductCatalog catalog({p});
    OrderStore orders(ordFile.string(), ordDetFile.string());
    BillStore bills(billFile.string(), billDetFile.string());

    Season season{"2026-11-20", "2026-11-30", 1000, true}; // 10% active
    SeasonService seasons({season});

    // Original paid order with quantity 2 pcs (2000)
    Order origOrder;
    origOrder.id = "00000722";
    origOrder.customerName = "Ali Khan";
    origOrder.date = "2026-11-22";
    origOrder.time = "14:05:09";
    origOrder.status = OrderStatus::Paid;
    origOrder.lines = { {"101", "123456", 2000} };
    origOrder.paymentAmount = 82800;
    origOrder.mode = PaymentMode::Cash;
    orders.orders().push_back(origOrder);

    // Original bill
    Bill origBill;
    origBill.id = "000000031";
    origBill.orderId = "00000722";
    origBill.customerName = "Ali Khan";
    origBill.date = "2026-11-22";
    origBill.time = "14:05:09";
    origBill.subtotal = 90000;
    origBill.tax = 4000;
    origBill.itemDiscount = 2000;
    origBill.seasonalDiscount = 9200;
    origBill.payable = 82800;
    origBill.received = 100000;
    origBill.balance = 17200;
    origBill.status = OrderStatus::Paid;
    origBill.lines = { {"MILKP", 45000, 2000, 2000} };
    bills.add(origBill);

    // Initial save to disk
    orders.save();
    bills.save();
    {
        std::ofstream out(prodFile);
        out << catalog.serialize();
    }

    CheckoutService service(catalog, orders, bills, prodFile.string(), seasons);
    OrderManager manager(catalog, &orders);

    // Cashier opens and edits paid order: quantity changed from 2 to 5 (5000)
    Order editedOrder = origOrder;
    editedOrder.lines = { {"101", "123456", 5000} };
    setTestActiveOrder(manager, editedOrder);

    // Save edited payment
    // 5 x 450 = 2250.00 (225000), tax 5 x 20 = 100.00 (10000), disc 5 x 10 = 50.00 (5000) -> base = 2300.00 (230000)
    // seasonal 10% = 230.00 (23000) -> payable = 2070.00 (207000)
    PaymentInfo payment;
    payment.mode = PaymentMode::Cash;
    payment.received = 210000; // 2100.00 PKR tendered
    payment.specialPercent = 0;
    payment.specialApproved = false;

    Bill outBill;
    OperationResult r = service.saveEdited(manager, payment, outBill);
    CHECK(r.success);

    // Stock was 10, increased quantity by (5 - 2) = 3, so stock becomes 10 - 3 = 7 (7000)
    auto pAfter = catalog.findById("101");
    CHECK(pAfter.has_value());
    CHECK_EQ(pAfter->available, 7000);

    // Same Bill ID, same date, same time
    CHECK_EQ(outBill.id, std::string("000000031"));
    CHECK_EQ(outBill.date, std::string("2026-11-22"));
    CHECK_EQ(outBill.time, std::string("14:05:09"));

    // Updated line quantity and totals
    CHECK_EQ(outBill.lines.size(), 1);
    CHECK_EQ(outBill.lines[0].quantity, 5000);
    CHECK_EQ(outBill.payable, 207000);
    CHECK_EQ(outBill.received, 210000);
    CHECK_EQ(outBill.balance, 3000); // 30.00 PKR change

    cleanFolder(tempDir);
}

int main() {
    testCashPaymentChange1000For828();
    testCardWithoutMerchantRefused();
    testSpecialWithoutApprovalRefused();
    testForcedFailureRollback();
    testSaveEditedStockDifference();
    return testSummary("test_checkout");
}
