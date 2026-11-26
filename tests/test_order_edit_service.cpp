#include "OrderEditService.h"
#include "Bill.h"
#include "BillStore.h"
#include "OrderManager.h"
#include "OrderStore.h"
#include "ProductCatalog.h"
#include "TestHelpers.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string readWholeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int main() {
    fs::path tempDir = fs::temp_directory_path() / "pos_test_order_edit_service";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir);

    std::string orderPath = (tempDir / "Order.txt").string();
    std::string detailPath = (tempDir / "OrderDetail.txt").string();
    std::string billPath = (tempDir / "Bill.txt").string();

    ProductCatalog catalog({
        {"P101", "111001", "P101", "Tea Box", MeasureUnit::Count, 20000, 0, 0, 10000, ""},
        {"P102", "111002", "P102", "Coffee Jar", MeasureUnit::Count, 50000, 0, 0, 5000, ""}
    });

    OrderStore orders(orderPath, detailPath);
    BillStore bills(billPath);

    Order archivedOrder;
    archivedOrder.id = "00000122";
    archivedOrder.date = "2026-11-22";
    archivedOrder.time = "10:00:00";
    archivedOrder.customerName = "Cancelled Customer";
    archivedOrder.status = OrderStatus::Archived;
    archivedOrder.lines.push_back({"P101", "111001", 1000});

    Order paidOrder;
    paidOrder.id = "00000222";
    paidOrder.date = "2026-11-22";
    paidOrder.time = "11:00:00";
    paidOrder.customerName = "Paid Customer";
    paidOrder.status = OrderStatus::Paid;
    paidOrder.paymentAmount = 40000;
    paidOrder.mode = PaymentMode::Cash;
    paidOrder.lines.push_back({"P101", "111001", 2000});

    Bill paidBill;
    paidBill.id = "000000001";
    paidBill.orderId = "00000222";
    paidBill.customerName = "Paid Customer";
    paidBill.date = "2026-11-22";
    paidBill.time = "11:00:00";
    paidBill.status = OrderStatus::Paid;
    paidBill.payable = 40000;

    Order heldOrder;
    heldOrder.id = "00000322";
    heldOrder.date = "2026-11-22";
    heldOrder.time = "12:00:00";
    heldOrder.customerName = "Held Customer";
    heldOrder.status = OrderStatus::OnHold;
    heldOrder.lines.push_back({"P102", "111002", 1000});

    CHECK(orders.save(archivedOrder).success);
    CHECK(orders.save(paidOrder).success);
    CHECK(orders.save(heldOrder).success);
    bills.addBill(paidBill);
    CHECK(bills.save().success);

    const std::string orderDiskBefore = readWholeFile(orderPath);
    const std::string detailDiskBefore = readWholeFile(detailPath);

    OrderManager manager(catalog, &orders);

    // 1. Archived order is always refused (even with manager approval)
    auto rArch = OrderEditService::open(manager, orders, bills, "00000122", "2026-11-22", true);
    CHECK(!rArch.success);
    CHECK(!manager.current().has_value());
    CHECK(!manager.original().has_value());

    // 2. Paid order without manager approval is refused
    auto rNoAuth = OrderEditService::open(manager, orders, bills, "00000222", "2026-11-25", false);
    CHECK(!rNoAuth.success);
    CHECK(!manager.current().has_value());
    CHECK(!manager.original().has_value());

    // 3. Paid order on day 8 with approval is refused
    auto rDay8 = OrderEditService::open(manager, orders, bills, "00000222", "2026-11-30", true);
    CHECK(!rDay8.success);
    CHECK(!manager.current().has_value());
    CHECK(!manager.original().has_value());

    // 4. Paid order on day 7 with approval opens in memory with original() set; disk stays unchanged
    auto rDay7 = OrderEditService::open(manager, orders, bills, "00000222", "2026-11-29", true);
    CHECK(rDay7.success);
    CHECK(manager.current().has_value());
    CHECK(manager.original().has_value());
    CHECK_EQ(manager.current()->id, std::string("00000222"));
    CHECK_EQ(manager.original()->id, std::string("00000222"));

    // Modify in-memory copy and discard -> disk and OrderStore remain untouched
    CHECK(manager.setQuantity("P101", 5000).success);
    CHECK_EQ(manager.current()->lines[0].quantity, 5000);
    CHECK_EQ(manager.original()->lines[0].quantity, 2000);
    CHECK_EQ(readWholeFile(orderPath), orderDiskBefore);
    CHECK_EQ(readWholeFile(detailPath), detailDiskBefore);

    auto rDiscard = OrderEditService::discard(manager);
    CHECK(rDiscard.success);
    CHECK(!manager.current().has_value());
    CHECK(!manager.original().has_value());
    CHECK_EQ(orders.findById("00000222")->lines[0].quantity, 2000);
    CHECK_EQ(readWholeFile(orderPath), orderDiskBefore);
    CHECK_EQ(readWholeFile(detailPath), detailDiskBefore);

    // 5. On-hold order opens like resume (original() remains empty)
    auto rHeld = OrderEditService::open(manager, orders, bills, "00000322", "2026-11-29", false);
    CHECK(rHeld.success);
    CHECK(manager.current().has_value());
    CHECK(!manager.original().has_value());
    CHECK_EQ(manager.current()->id, std::string("00000322"));
    CHECK(manager.current()->status == OrderStatus::InProgress);

    // 6. Simulated crash: reload OrderStore from disk -> in-progress order becomes on-hold,
    //    and any unsaved paid-order edit is gone while paid order stays intact.
    {
        OrderEditService::open(manager, orders, bills, "00000222", "2026-11-29", true);
        CHECK(manager.current().has_value());
        CHECK(manager.setQuantity("P101", 4000).success);
        // "Crash" here (manager goes out of scope without saving paid edit; 00000322 was InProgress on disk)
    }

    OrderStore crashedReload(orderPath, detailPath);
    CHECK(crashedReload.load().success);
    auto reloadedHeld = crashedReload.findById("00000322");
    CHECK(reloadedHeld.has_value());
    CHECK(reloadedHeld->status == OrderStatus::OnHold);

    auto reloadedPaid = crashedReload.findById("00000222");
    CHECK(reloadedPaid.has_value());
    CHECK(reloadedPaid->status == OrderStatus::Paid);
    CHECK_EQ(reloadedPaid->lines[0].quantity, 2000);

    fs::remove_all(tempDir, ec);
    return testSummary("test_order_edit_service");
}
