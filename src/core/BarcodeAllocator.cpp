#include "BarcodeAllocator.h"
#include "BarcodeGenerator.h"
#include "CodeValidator.h"

OperationResult BarcodeAllocator::nextBarcode(const std::string& categoryId, const ProductCatalog& catalog) const
{
    OperationResult r;
    if (categoryId.size() != 2 || categoryId[0] < '1' || categoryId[0] > '9' ||
        categoryId[1] < '0' || categoryId[1] > '9') {
        r.message = "Category id must be 2 digits from 10 to 99.";
        return r;
    }
    int highest = 0;
    for (auto& p : catalog.all()) {
        if (p.barcode.size() != 6 || p.barcode.compare(0, 2, categoryId) != 0) continue;
        int serial = std::stoi(p.barcode.substr(2, 4));
        if (serial > highest) highest = serial;
    }
    if (highest >= 9999) { r.message = "No free barcode left in this category."; return r; }

    std::string serial = std::to_string(highest + 1);
    while (serial.size() < 4) serial = "0" + serial;
    std::string code = categoryId + serial;
    if (!isValidSixDigitCode(code)) { r.message = "Generated barcode is not valid."; return r; }

    r.success = true;
    r.value = code;
    r.message = "Next barcode " + code;
    return r;
}

OperationResult BarcodeAllocator::makeImage(const std::string& barcode, const std::string& outputBmpPath) const
{
    return generateBarcode(barcode, outputBmpPath);
}