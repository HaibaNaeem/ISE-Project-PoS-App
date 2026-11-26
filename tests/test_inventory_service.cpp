#include "InventoryService.h"
#include "ProductCatalog.h"
#include "TestHelpers.h"
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    fs::path tempDir = fs::temp_directory_path() / "pos_test_inventory_service";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir);

    std::string prodPath = (tempDir / "Product.txt").string();

    Product p1{"101", "123456", "MILKP", "Milk Powder 400g", MeasureUnit::Count, 45000, 2000, 1000, 10000, "", "12"};
    Product p2{"102", "234567", "SUGAR", "White Sugar", MeasureUnit::Weight, 16000, 0, 0, 2500, "", "23"};
    ProductCatalog catalog({p1, p2});
    CHECK(catalog.save(prodPath).success);

    InventoryService service(catalog, prodPath);
    CHECK_EQ(service.list().size(), static_cast<size_t>(2));

    // 1. setAvailable works (including setting to 0)
    auto rSet = service.setAvailable("101", 7000);
    CHECK(rSet.success);
    CHECK_EQ(catalog.findById("101")->available, Quantity(7000));

    auto rZero = service.setAvailable("101", 0);
    CHECK(rZero.success);
    CHECK_EQ(catalog.findById("101")->available, Quantity(0));

    // 2. receiveStock adds to existing stock (including fractional weight)
    auto rRec1 = service.receiveStock("101", 5000);
    CHECK(rRec1.success);
    CHECK_EQ(catalog.findById("101")->available, Quantity(5000));

    auto rRecWeight = service.receiveStock("102", 1250);
    CHECK(rRecWeight.success);
    CHECK_EQ(catalog.findById("102")->available, Quantity(3750));

    // 3. Negative / invalid quantities rejected without modifying stock
    CHECK(!service.setAvailable("101", -1000).success);
    CHECK_EQ(catalog.findById("101")->available, Quantity(5000));

    CHECK(!service.receiveStock("101", -2000).success);
    CHECK(!service.receiveStock("101", 0).success);
    CHECK_EQ(catalog.findById("101")->available, Quantity(5000));

    // Count product rejects non-whole quantities
    CHECK(!service.setAvailable("101", 1500).success);
    CHECK(!service.receiveStock("101", 1500).success);
    CHECK_EQ(catalog.findById("101")->available, Quantity(5000));

    // Unknown product rejected
    CHECK(!service.setAvailable("999", 1000).success);
    CHECK(!service.receiveStock("999", 1000).success);

    // 4. Reload Product.txt into a fresh ProductCatalog -> changes survived restart
    ProductCatalog reloaded;
    CHECK(reloaded.load(prodPath).success);
    CHECK(reloaded.findById("101").has_value());
    CHECK_EQ(reloaded.findById("101")->available, Quantity(5000));
    CHECK(reloaded.findById("102").has_value());
    CHECK_EQ(reloaded.findById("102")->available, Quantity(3750));

    fs::remove_all(tempDir, ec);
    return testSummary("test_inventory_service");
}
