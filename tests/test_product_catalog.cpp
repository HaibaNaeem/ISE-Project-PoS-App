#include <filesystem>
#include <fstream>
#include <iostream>
#include "ProductCatalog.h"

static int failed = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "FAIL line " << __LINE__ << ": " #c "\n"; ++failed; } } while (0)
static void writeFile(const std::string& p, const std::string& s) { std::ofstream(p, std::ios::binary) << s; }
static std::string readAll(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::string raw{std::istreambuf_iterator<char>(f), {}};
    std::string out;
    out.reserve(raw.size());
    for (char ch : raw) {
        if (ch != '\r') out.push_back(ch);
    }
    return out;
}

static Product make(const std::string& barcode, const std::string& acr)
{
    Product p;
    p.barcode = barcode; p.categoryId = barcode.substr(0, 2);
    p.acronym = acr; p.name = "Item " + acr;
    p.unitPrice = 10000; p.available = 5000;
    return p;
}

int main()
{
    ProductCatalog c;
    CHECK(!c.load("no_such_file.txt").success);

    writeFile("pc_empty.txt", "");
    CHECK(c.load("pc_empty.txt").success && c.all().empty());

    writeFile("pc_mixed.txt",
        "101 123456 12 MILKP \"Milk Powder 400g\" count 450.00 20.00 10.00 12 \"img/a.bmp\" #\n"
        "102 123456 12 DUPBC \"Dup barcode\" count 1.00 0.00 0.00 1 \"\" #\n"
        "103 012345 01 BADBC \"Zero first\" count 1.00 0.00 0.00 1 \"\" #\n"
        "104 200145 20 SUGAR \"Sugar\" weight 160.00 5.00 0.00 25.500 \"\" #\n"
        "105 300000 30 TOOLONGACR \"X\" count 1.00 0.00 0.00 1 \"\" #\n"
        "106 400000 40 NEG \"Neg\" count -5.00 0.00 0.00 1 \"\" #\n"
        "107 500000 50 NOHASH \"No hash\" count 1.00 0.00 0.00 1 \"\"\n"
        "108 600000 60 KG \"Bad unit\" liters 1.00 0.00 0.00 1 \"\" #\n"
        "109 700000 71 CATBAD \"Category mismatch\" count 1.00 0.00 0.00 1 \"\" #\n"
        "110 800000 80 MILKP \"Dup acronym\" count 1.00 0.00 0.00 1 \"\" #\n"
        "111 900000 SPACE \"Old format\" count 1.00 0.00 0.00 1 \"\" #\n");
    CHECK(c.load("pc_mixed.txt").success);
    CHECK(c.all().size() == 2);
    CHECK(c.warnings().size() == 9);
    CHECK(c.findByBarcode("123456") && c.findByBarcode("123456")->categoryId == "12");
    CHECK(c.findById("104") && c.findById("104")->available == 25500);

    CHECK(c.save("pc_saved.txt").success);
    ProductCatalog c2;
    CHECK(c2.load("pc_saved.txt").success && c2.warnings().empty() && c2.all().size() == 2);
    CHECK(readAll("pc_saved.txt") == c2.serialize());

    CHECK(c2.reduceStock("101", 2000).success && c2.findById("101")->available == 10000);
    CHECK(!c2.reduceStock("101", 999000).success);
    CHECK(!c2.reduceStock("nope", 1000).success);

    // add / update / locks
    ProductCatalog m;
    CHECK(m.nextProductId() == "101");
    CHECK(m.add(make("120001", "A1")).success);
    CHECK(m.add(make("120002", "A2")).success);
    CHECK(m.all()[1].id == "102");
    CHECK(!m.add(make("120001", "A3")).success);            // duplicate barcode
    CHECK(!m.add(make("120003", "A1")).success);            // duplicate acronym
    Product bad = make("120004", "A4"); bad.categoryId = "13";
    CHECK(!m.add(bad).success);                             // category mismatch
    Product neg = make("120005", "A5"); neg.available = -1;
    CHECK(!m.add(neg).success);

    Product u = *m.findById("101");
    u.name = "Renamed"; u.unitPrice = 20000;
    CHECK(m.update(u).success && m.findById("101")->name == "Renamed");
    Product lockB = u; lockB.barcode = "120009";
    CHECK(!m.update(lockB).success);
    Product lockC = u; lockC.categoryId = "13";
    CHECK(!m.update(lockC).success);
    Product dupA = u; dupA.acronym = "A2";
    CHECK(!m.update(dupA).success);

    CHECK(m.setAvailable("101", 7000).success && m.findById("101")->available == 7000);
    CHECK(!m.setAvailable("101", -1).success);
    CHECK(m.findById("101")->available == 7000);

    // real sample file
    ProductCatalog s;
    if (s.load("sample_data/Product.txt").success && !s.all().empty()) {
        CHECK(s.warnings().empty());
        CHECK(s.save("pc_sample_copy.txt").success);
        CHECK(readAll("pc_sample_copy.txt") == readAll("sample_data/Product.txt"));
    } else {
        std::cout << "SKIP: sample_data/Product.txt not found or empty from this folder\n";
    }

    for (auto f : {"pc_empty.txt", "pc_mixed.txt", "pc_saved.txt", "pc_sample_copy.txt"})
        std::filesystem::remove(f);
    std::cout << (failed ? "FAILED\n" : "All product catalog tests passed\n");
    return failed ? 1 : 0;
}