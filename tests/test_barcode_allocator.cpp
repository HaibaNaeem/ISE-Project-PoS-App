#include <filesystem>
#include <iostream>
#include "BarcodeAllocator.h"
#include "CodeValidator.h"

static int failed = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "FAIL line " << __LINE__ << ": " #c "\n"; ++failed; } } while (0)

static Product withBarcode(const std::string& b, const std::string& acr)
{
    Product p; p.barcode = b; p.categoryId = b.substr(0, 2); p.acronym = acr; p.name = acr;
    return p;
}

int main()
{
    BarcodeAllocator a;
    ProductCatalog cat({withBarcode("120001", "A1"), withBarcode("120002", "A2")});

    auto r = a.nextBarcode("12", cat);
    CHECK(r.success && r.value == "120003");
    r = a.nextBarcode("15", cat);                         // empty category
    CHECK(r.success && r.value == "150001" && isValidSixDigitCode(r.value));
    CHECK(!a.nextBarcode("5", cat).success);
    CHECK(!a.nextBarcode("AB", cat).success);

    ProductCatalog full({withBarcode("129999", "F1")});
    CHECK(!a.nextBarcode("12", full).success);

    // gap: highest + 1, not the first free
    ProductCatalog gap({withBarcode("120001", "G1"), withBarcode("120005", "G2")});
    CHECK(a.nextBarcode("12", gap).value == "120006");

    std::filesystem::create_directories("alloc_test");
    auto img = a.makeImage("120003", "alloc_test/120003.bmp");
    CHECK(img.success && std::filesystem::exists("alloc_test/120003.bmp"));
    std::filesystem::remove_all("alloc_test");

    std::cout << (failed ? "FAILED\n" : "All barcode allocator tests passed\n");
    return failed ? 1 : 0;
}