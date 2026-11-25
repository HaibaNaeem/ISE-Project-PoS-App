#pragma once
// OWNER: Member 3. Inventory stock queries and updates persisted to Product.txt via FileTransaction.
#include <string>
#include <vector>
#include "CommonTypes.h"
#include "Product.h"
#include "ProductCatalog.h"

class InventoryService {
public:
    InventoryService(ProductCatalog& catalog, std::string productFilePath);

    const std::vector<Product>& list() const;

    // Sets available stock for productId (>= 0) and saves Product.txt via FileTransaction.
    OperationResult setAvailable(const std::string& productId, Quantity quantity);

    // Adds received stock (> 0) to productId and saves Product.txt via FileTransaction.
    OperationResult receiveStock(const std::string& productId, Quantity quantity);

private:
    OperationResult updateAvailable(const std::string& productId, Quantity targetQuantity, bool isReceive);

    ProductCatalog* catalog_;
    std::string productFilePath_;
};
