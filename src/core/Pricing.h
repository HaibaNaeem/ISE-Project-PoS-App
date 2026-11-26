#pragma once
// OWNER: Member 4. Pure calculation, no files, no Qt.
// All financial math for pricing lines, calculating bills, and change due.
#include <string>
#include <vector>
#include <memory>
#include "Bill.h"
#include "DiscountRules.h"
#include "ProductCatalog.h"

// Price one line.
// Formula: subtotal = price x qty, tax = tax x qty, discount = discount x qty.
BillLine priceLine(const Product& product, Quantity quantity);

// Build the complete bill for an order with all line calculations, discount rules, and grand totals.
// If a product is not found in the catalog, it is handled gracefully without crashing.
Bill buildBill(const Order& order, const ProductCatalog& catalog, const BillOptions& options);

// Overload allowing custom discount rules for extensibility (OCP)
Bill buildBill(const Order& order, const ProductCatalog& catalog, const BillOptions& options,
               const std::vector<std::shared_ptr<IDiscountRule>>& rules);

// Backward-compatible overload from Increment 2 (uses default BillOptions with 0% discounts)
Bill buildBill(const Order& order, const ProductCatalog& catalog, const std::string& billId);

// Change to give back for a cash payment: tendered - payable (returns 0 if tendered <= payable).
Money changeDue(Money payable, Money tendered);

// Convenient alias matching contract terminology
inline Money calculateChange(Money payable, Money tendered) {
    return changeDue(payable, tendered);
}
