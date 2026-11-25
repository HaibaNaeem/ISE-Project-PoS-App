
#pragma once

// OWNER: Member 2.
// Product.txt (11 fields:
// id barcode categoryId acronym name unit price tax discount qty image #).

#include <optional>
#include <string>
#include <vector>

#include "CommonTypes.h"
#include "Product.h"

class ProductCatalog {
public:
    ProductCatalog() = default;

    explicit ProductCatalog(std::vector<Product> products);

    OperationResult load(const std::string& path);
    OperationResult save(const std::string& path) const;

    std::string serialize() const;

    std::optional<Product> findByBarcode(const std::string& barcode) const;
    std::optional<Product> findById(const std::string& id) const;

    const std::vector<Product>& all() const {
        return products_;
    }

    const std::vector<std::string>& warnings() const {
        return warnings_;
    }

    // Used by CheckoutService at payment time.
    OperationResult reduceStock(const std::string& productId, Quantity amount);

    // Increment 3
    OperationResult add(const Product& p);
    OperationResult update(const Product& p);
    OperationResult setAvailable(const std::string& productId, Quantity q);

    std::string nextProductId() const;

private:
    OperationResult checkFields(const Product& p) const;

    bool acronymTaken(
        const std::string& acronym,
        const std::string& exceptId
    ) const;

    std::vector<Product> products_;
    std::vector<std::string> warnings_;
};
