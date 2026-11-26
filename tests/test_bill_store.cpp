// Tests for Member 4 BillStore module (Increment 3)
// Format: Bill.txt (one row per bill) and BillDetail.txt (one row per product line)
#include "TestHelpers.h"
#include "BillStore.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static void cleanFolder(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

static std::string readWholeFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void testEmptyAndMissingFileNextId() {
    fs::path tempDir = fs::temp_directory_path() / "test_bill_store_empty_inc3";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path missingBill = tempDir / "NonExistentBill.txt";
    fs::path missingDetail = tempDir / "NonExistentDetail.txt";
    BillStore store(missingBill.string(), missingDetail.string());

    OperationResult r = store.load();
    CHECK(r.success);
    CHECK_EQ(store.bills().size(), 0);
    CHECK_EQ(store.nextBillId(), std::string("000000001"));

    cleanFolder(tempDir);
}

static void testNewFormatLoad() {
    fs::path tempDir = fs::temp_directory_path() / "test_bill_store_new_format";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path billFile = tempDir / "Bill.txt";
    fs::path detailFile = tempDir / "BillDetail.txt";

    {
        std::ofstream out(billFile, std::ios::binary);
        out << "000000031 \"Ali Khan\" 00000722 900.00 40.00 20.00 828.00 1000.00 172.00 paid 92.00 0.00 2026-11-22 14:05:09 #\n";
    }
    {
        std::ofstream out(detailFile, std::ios::binary);
        out << "000000031 MILKP 450.00 2 20.00 #\n";
    }

    BillStore store(billFile.string(), detailFile.string());
    OperationResult r = store.load();
    CHECK(r.success);
    CHECK_EQ(store.bills().size(), 1);

    const Bill& b = store.bills()[0];
    CHECK_EQ(b.id, std::string("000000031"));
    CHECK_EQ(b.customerName, std::string("Ali Khan"));
    CHECK_EQ(b.orderId, std::string("00000722"));
    CHECK_EQ(b.subtotal, 90000);
    CHECK_EQ(b.tax, 4000);
    CHECK_EQ(b.itemDiscount, 2000);
    CHECK_EQ(b.payable, 82800);
    CHECK_EQ(b.received, 100000);
    CHECK_EQ(b.balance, 17200);
    CHECK(b.status == OrderStatus::Paid);
    CHECK_EQ(b.seasonalDiscount, 9200);
    CHECK_EQ(b.specialDiscount, 0);
    CHECK_EQ(b.date, std::string("2026-11-22"));
    CHECK_EQ(b.time, std::string("14:05:09"));

    // Lines from BillDetail
    CHECK_EQ(b.lines.size(), 1);
    CHECK_EQ(b.lines[0].acronym, std::string("MILKP"));
    CHECK_EQ(b.lines[0].unitPrice, 45000);
    CHECK_EQ(b.lines[0].quantity, 2000);
    CHECK_EQ(b.lines[0].discount, 2000);

    CHECK_EQ(store.nextBillId(), std::string("000000032"));

    cleanFolder(tempDir);
}

static void testDuplicateIdSkipped() {
    fs::path tempDir = fs::temp_directory_path() / "test_bill_store_duplicate";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path billFile = tempDir / "Bill.txt";
    fs::path detailFile = tempDir / "BillDetail.txt";

    {
        std::ofstream out(billFile, std::ios::binary);
        out << "000000001 \"First Customer\" 00000101 100.00 0.00 0.00 100.00 100.00 0.00 paid 0.00 0.00 2026-11-22 10:00:00 #\n";
        out << "000000001 \"Duplicate Customer\" 00000102 200.00 0.00 0.00 200.00 200.00 0.00 paid 0.00 0.00 2026-11-22 11:00:00 #\n";
    }
    {
        std::ofstream out(detailFile, std::ios::binary);
        out << "000000001 PROD 100.00 1 0.00 #\n";
    }

    BillStore store(billFile.string(), detailFile.string());
    OperationResult r = store.load();
    // Warning contains line number and mentions duplicate
    CHECK(r.message.find("line 2") != std::string::npos || r.message.find("Line 2") != std::string::npos);
    CHECK(r.message.find("Duplicate") != std::string::npos || r.message.find("duplicate") != std::string::npos);
    CHECK_EQ(store.bills().size(), 1);
    CHECK_EQ(store.bills()[0].customerName, std::string("First Customer"));

    cleanFolder(tempDir);
}

static void testRoundTripSaveAndLoad() {
    fs::path tempDir = fs::temp_directory_path() / "test_bill_store_roundtrip_inc3";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path origBill = tempDir / "OrigBill.txt";
    fs::path origDetail = tempDir / "OrigDetail.txt";
    fs::path savedBill = tempDir / "SavedBill.txt";
    fs::path savedDetail = tempDir / "SavedDetail.txt";

    {
        std::ofstream out(origBill, std::ios::binary);
        out << "000000001 \"Customer One\" 00000101 200.00 20.00 10.00 210.00 250.00 40.00 paid 0.00 0.00 2026-11-22 10:00:00 #\n";
        out << "000000002 \"Customer Two\" 00000102 50.00 5.00 0.00 55.00 55.00 0.00 paid 0.00 0.00 2026-11-22 11:00:00 #\n";
    }
    {
        std::ofstream out(origDetail, std::ios::binary);
        out << "000000001 ITEM1 100.00 2 10.00 #\n";
        out << "000000002 ITEM2 40.00 1.250 0.00 #\n";
    }

    BillStore store1(origBill.string(), origDetail.string());
    CHECK(store1.load().success);
    CHECK(store1.save(savedBill.string(), savedDetail.string()).success);

    BillStore store2(savedBill.string(), savedDetail.string());
    CHECK(store2.load().success);

    CHECK_EQ(store2.bills().size(), 2);
    CHECK_EQ(store2.bills()[0].id, store1.bills()[0].id);
    CHECK_EQ(store2.bills()[0].payable, store1.bills()[0].payable);
    CHECK_EQ(store2.bills()[1].lines.size(), 1);
    CHECK_EQ(store2.bills()[1].lines[0].quantity, 1250); // weight survived

    CHECK_EQ(readWholeFile(savedBill), readWholeFile(origBill));
    CHECK_EQ(readWholeFile(savedDetail), readWholeFile(origDetail));

    cleanFolder(tempDir);
}

static void testOldIncrement2FormatRowGivesErrorWithLineNumber() {
    fs::path tempDir = fs::temp_directory_path() / "test_bill_store_legacy";
    cleanFolder(tempDir);
    fs::create_directories(tempDir);

    fs::path legacyBill = tempDir / "LegacyBill.txt";
    {
        std::ofstream out(legacyBill, std::ios::binary);
        // Valid line 1
        out << "000000001 \"Valid Customer\" 00000101 100.00 0.00 0.00 100.00 100.00 0.00 paid 0.00 0.00 2026-11-22 10:00:00 #\n";
        // Old Increment 2 format on line 2 (items comma separated, 9 fields total)
        out << "000000002 \"Old Customer\" 00000102 ITEM1,100.00,0.00,1 100.00 0.00 0.00 100.00 paid #\n";
    }

    BillStore store(legacyBill.string(), "");
    OperationResult r = store.load();
    // Must report error with line number for line 2
    CHECK(r.message.find("line 2") != std::string::npos || r.message.find("Line 2") != std::string::npos);
    // Bad line skipped, valid line 1 kept
    CHECK_EQ(store.bills().size(), 1);
    CHECK_EQ(store.bills()[0].id, std::string("000000001"));

    cleanFolder(tempDir);
}

static void testFindAndReplace() {
    BillStore store;
    Bill b1;
    b1.id = "000000001";
    b1.orderId = "ORD01";
    b1.payable = 10000;
    store.add(b1);

    auto found = store.findById("000000001");
    CHECK(found.has_value());
    CHECK_EQ(found->payable, 10000);

    auto foundOrd = store.findByOrderId("ORD01");
    CHECK(foundOrd.has_value());

    // Replace
    b1.payable = 15000;
    OperationResult rep = store.replace(b1);
    CHECK(rep.success);

    auto updated = store.findById("000000001");
    CHECK(updated.has_value());
    CHECK_EQ(updated->payable, 15000);

    // Non-existent replace
    Bill b2;
    b2.id = "999999999";
    CHECK(!store.replace(b2).success);
}

int main() {
    testEmptyAndMissingFileNextId();
    testNewFormatLoad();
    testDuplicateIdSkipped();
    testRoundTripSaveAndLoad();
    testOldIncrement2FormatRowGivesErrorWithLineNumber();
    testFindAndReplace();
    return testSummary("test_bill_store");
}
