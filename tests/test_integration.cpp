// OWNER: Member 5. End-to-end Increment 3 integration test (no Qt window).
#include "BarcodeAllocator.h"
#include "BarcodeDecoder.h"
#include "BillStore.h"
#include "CategoryStore.h"
#include "CheckoutService.h"
#include "DataPaths.h"
#include "DateRange.h"
#include "ManagerAuth.h"
#include "OrderEditService.h"
#include "OrderManager.h"
#include "OrderStore.h"
#include "Pricing.h"
#include "ProductCatalog.h"
#include "ReportRegistry.h"
#include "SeasonService.h"
#include "SeasonStore.h"
#include "TestHelpers.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static void removeFolder(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

int main() {
    const fs::path root = fs::temp_directory_path() / "pos_v2_integration_run";
    removeFolder(root);
    fs::create_directories(root / "sample_data" / "barcodes");

    // Copy Increment 3 sample_data into isolated root so DataPaths can initialize data/
    const fs::path srcSamples(SAMPLE_DATA_DIR);
    for (const char* name : {"Product.txt", "PCategory.txt", "Seasons.txt", "Settings.txt",
                             "Order.txt", "OrderDetail.txt", "Bill.txt", "BillDetail.txt"}) {
        fs::copy_file(srcSamples / name, root / "sample_data" / name, fs::copy_options::overwrite_existing);
    }

    DataPaths paths(root.generic_string());
    CHECK(paths.ensureDataFolder().success);

    // 1. Load all stores + ManagerAuth
    ManagerAuth auth;
    CHECK(auth.load(paths.settingsFile()).success);
    CHECK(auth.check("1234"));
    CHECK(!auth.check("9999"));

    CategoryStore categories;
    CHECK(categories.load(paths.categoryFile()).success);
    CHECK_EQ(categories.all().size(), static_cast<std::size_t>(9));

    ProductCatalog catalog;
    CHECK(catalog.load(paths.productFile()).success);
    CHECK_EQ(catalog.all().size(), static_cast<std::size_t>(9));

    OrderStore orderStore(paths.orderFile(), paths.orderDetailFile());
    CHECK(orderStore.load().success);

    BillStore billStore(paths.billFile(), paths.billDetailFile());
    CHECK(billStore.load().success);

    SeasonStore seasonStore;
    CHECK(seasonStore.load(paths.seasonsFile()).success);
    CHECK_EQ(seasonStore.all().size(), static_cast<std::size_t>(2));

    // 2. Add category 15 "Snacks" and a new product using BarcodeAllocator
    CHECK(categories.add({"15", "Snacks", "SNACKS"}).success);
    CHECK(categories.save(paths.categoryFile()).success);

    BarcodeAllocator allocator;
    OperationResult nextBc = allocator.nextBarcode("15", catalog);
    CHECK(nextBc.success);
    CHECK_EQ(nextBc.value, std::string("150001"));

    const fs::path newBmpPath = root / "data" / "barcodes" / "150001.bmp";
    fs::create_directories(newBmpPath.parent_path());
    CHECK(allocator.makeImage(nextBc.value, newBmpPath.string()).success);
    OperationResult decoded = decodeBarcodeImage(newBmpPath.string());
    CHECK(decoded.success);
    CHECK_EQ(decoded.value, std::string("150001"));

    Product chips;
    chips.id = catalog.nextProductId();
    chips.barcode = nextBc.value;
    chips.categoryId = "15";
    chips.acronym = "CHIPS";
    chips.name = "Potato Chips";
    chips.unit = MeasureUnit::Count;
    chips.unitPrice = 10000;       // 100.00
    chips.taxPerUnit = 500;        // 5.00
    chips.discountPerUnit = 0;     // 0.00
    chips.available = 10000;       // 10
    chips.barcodeImagePath = "data/barcodes/150001.bmp";
    CHECK(catalog.add(chips).success);
    CHECK(catalog.save(paths.productFile()).success);

    // 3. Scan three barcodes with OrderManager (auto-starts order, clock inside active Nov season)
    OrderManager manager(catalog, &orderStore);
    manager.setClock([]() { return DateTime{"2026-11-22", "14:05:09", 22}; });

    CHECK(manager.addByBarcode("123456", 2000).success); // 2x Milk Powder: 2*(450+20-10) = 920.00
    CHECK(manager.addByBarcode("150001", 3000).success); // 3x Chips: 3*(100+5-0) = 315.00
    CHECK(manager.addByBarcode("200145", 1000).success); // 1kg Sugar: 1*(160+5-0) = 165.00
    CHECK(manager.current().has_value());
    CHECK_EQ(manager.current()->lines.size(), static_cast<std::size_t>(3));
    const std::string orderId = manager.current()->id;

    // 4. Pay in cash with active 10% seasonal discount
    SeasonService seasonService(seasonStore.all());
    CheckoutService checkout(catalog, orderStore, billStore, paths.productFile(), seasonService);

    PaymentInfo payInfo;
    payInfo.mode = PaymentMode::Cash;
    payInfo.received = 150000; // 1500.00 PKR

    Bill paidBill;
    OperationResult payRes = checkout.pay(manager, payInfo, paidBill);
    CHECK(payRes.success);
    CHECK(!manager.current().has_value());
    CHECK_EQ(paidBill.seasonalDiscount, Money(14000)); // 10% of 1400.00 = 140.00
    CHECK_EQ(paidBill.payable, Money(126000));         // 1260.00
    CHECK_EQ(paidBill.balance, Money(24000));          // 1500 - 1260 = 240.00
    CHECK_EQ(catalog.findById(chips.id)->available, Quantity(7000));

    // 5. Edit the paid order (within 7 days, manager approved), change Chips qty 3 -> 5
    OperationResult openRes = OrderEditService::open(
        manager, orderStore, billStore, orderId, "2026-11-25", true);
    CHECK(openRes.success);
    CHECK(manager.original().has_value());
    CHECK(manager.setQuantity(chips.id, 5000).success); // diff = +2; 7 + 3 >= 5

    PaymentInfo editPayInfo;
    editPayInfo.mode = PaymentMode::Cash;
    editPayInfo.received = 200000; // 2000.00 PKR

    Bill editedBill;
    OperationResult editSaveRes = checkout.saveEdited(manager, editPayInfo, editedBill);
    CHECK(editSaveRes.success);
    CHECK_EQ(editedBill.id, paidBill.id);
    CHECK_EQ(editedBill.date, paidBill.date);
    CHECK_EQ(editedBill.seasonalDiscount, Money(16100)); // 10% of 1610.00 = 161.00
    CHECK_EQ(editedBill.payable, Money(144900));         // 1449.00
    CHECK_EQ(editedBill.balance, Money(55100));          // 2000 - 1449 = 551.00
    CHECK_EQ(catalog.findById(chips.id)->available, Quantity(5000)); // 7 - 2 = 5

    // 6. Reload everything from disk and check all files
    CategoryStore reloadedCats;
    CHECK(reloadedCats.load(paths.categoryFile()).success);
    CHECK_EQ(reloadedCats.all().size(), static_cast<std::size_t>(10));
    CHECK(reloadedCats.findById("15").has_value());

    ProductCatalog reloadedCatalog;
    CHECK(reloadedCatalog.load(paths.productFile()).success);
    CHECK_EQ(reloadedCatalog.all().size(), static_cast<std::size_t>(10));
    CHECK_EQ(reloadedCatalog.findById(chips.id)->available, Quantity(5000));
    CHECK_EQ(reloadedCatalog.findById("101")->available, Quantity(10000));
    CHECK_EQ(reloadedCatalog.findById("103")->available, Quantity(24500));

    OrderStore reloadedOrders(paths.orderFile(), paths.orderDetailFile());
    CHECK(reloadedOrders.load().success);
    CHECK_EQ(reloadedOrders.orders().size(), static_cast<std::size_t>(1));
    CHECK(reloadedOrders.orders()[0].status == OrderStatus::Paid);
    CHECK_EQ(reloadedOrders.orders()[0].paymentAmount, Money(144900));

    BillStore reloadedBills(paths.billFile(), paths.billDetailFile());
    CHECK(reloadedBills.load().success);
    CHECK_EQ(reloadedBills.bills().size(), static_cast<std::size_t>(1));
    CHECK_EQ(reloadedBills.bills()[0].id, paidBill.id);
    CHECK_EQ(reloadedBills.bills()[0].lines.size(), static_cast<std::size_t>(3));
    CHECK_EQ(reloadedBills.bills()[0].payable, Money(144900));

    SeasonStore reloadedSeasons;
    CHECK(reloadedSeasons.load(paths.seasonsFile()).success);
    CHECK_EQ(reloadedSeasons.all().size(), static_cast<std::size_t>(2));

    // 7. Run all six reports on reloaded data
    ReportRegistry registry = createDefaultReportRegistry();
    CHECK_EQ(registry.all().size(), static_cast<std::size_t>(6));

    ReportData reportData{reloadedCatalog, reloadedOrders.orders(), reloadedBills.all()};
    DateRange novRange = rangeFor(Period::Last7Days, "2026-11-26");

    std::vector<ReportTable> tables;
    for (const auto& rep : registry.all()) {
        tables.push_back(rep->build(reportData, novRange));
    }
    CHECK_EQ(tables[0].rows.size(), static_cast<std::size_t>(0));  // Incomplete orders
    CHECK_EQ(tables[1].rows.size(), static_cast<std::size_t>(1));  // Completed orders
    CHECK_EQ(tables[2].rows.size(), static_cast<std::size_t>(1));  // Cash sales
    CHECK_EQ(tables[2].totalsRow[3], std::string("1449.00"));
    CHECK_EQ(tables[3].rows.size(), static_cast<std::size_t>(1));  // All sales
    CHECK_EQ(tables[4].rows.size(), static_cast<std::size_t>(3));  // Top 10 sold
    CHECK_EQ(tables[4].rows[0][0], std::string("CHIPS"));          // 5 sold -> #1
    CHECK_EQ(tables[5].rows.size(), static_cast<std::size_t>(10)); // Top 10 in stock

    removeFolder(root);
    return testSummary("test_integration");
}
