#pragma once
#include <string>
#include "CommonTypes.h"
#include "ProductCatalog.h"

class BarcodeAllocator {
public:
    // value = category id + 4-digit serial, e.g. "120003"
    OperationResult nextBarcode(const std::string& categoryId, const ProductCatalog& catalog) const;
    // calls generateBarcode(); saves the BMP at outputBmpPath
    OperationResult makeImage(const std::string& barcode, const std::string& outputBmpPath) const;
};