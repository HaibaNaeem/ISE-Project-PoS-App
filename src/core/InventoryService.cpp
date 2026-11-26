// OWNER: Member 3. Inventory stock queries and updates persisted to Product.txt via FileTransaction.
#include "InventoryService.h"
#include "RecordFormat.h"
#include <limits>
#include <utility>

InventoryService::InventoryService(ProductCatalog& catalog, std::string productFilePath)
    : catalog_(&catalog), productFilePath_(std::move(productFilePath)) {}

const std::vector<Product>& InventoryService::list() const {
    return catalog_->all();
}

OperationResult InventoryService::setAvailable(const std::string& productId, Quantity quantity) {
    if (quantity < 0) {
        return {false, "Available stock cannot be negative."};
    }
    return updateAvailable(productId, quantity, false);
}

OperationResult InventoryService::receiveStock(const std::string& productId, Quantity quantity) {
    if (quantity <= 0) {
        return {false, "Received quantity must be greater than zero."};
    }
    return updateAvailable(productId, quantity, true);
}

OperationResult InventoryService::updateAvailable(const std::string& productId,
                                                  Quantity quantityArg,
                                                  bool isReceive) {
    std::vector<Product> updated = catalog_->all();
    bool found = false;
    std::string productName;
    Quantity newAvailable = 0;

    for (auto& product : updated) {
        if (product.id != productId) {
            continue;
        }
        found = true;
        productName = product.name;

        if (product.unit == MeasureUnit::Count && (quantityArg % 1000 != 0)) {
            return {false, "Count products require whole quantities (e.g., 1, 2, 3)."};
        }

        if (isReceive) {
            if (product.available > std::numeric_limits<Quantity>::max() - quantityArg) {
                return {false, "Stock quantity overflow for product '" + product.name + "'."};
            }
            newAvailable = product.available + quantityArg;
        } else {
            newAvailable = quantityArg;
        }

        if (newAvailable < 0) {
            return {false, "Available stock cannot go below zero."};
        }
        product.available = newAvailable;
        break;
    }

    if (!found) {
        return {false, "Product not found: " + productId};
    }

    // TODO(M2): replace with catalog_->setAvailable(productId, newAvailable) when merged.
    ProductCatalog candidate(std::move(updated));

    if (!productFilePath_.empty()) {
        FileTransaction tx;
        tx.add(productFilePath_, candidate.serialize());
        OperationResult txRes = tx.commit();
        if (!txRes.success) {
            return txRes;
        }
    }

    *catalog_ = std::move(candidate);
    return {true,
            (isReceive ? "Stock received for " : "Stock updated for ") + productName + ".",
            productId};
}
