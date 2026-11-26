// OWNER: Member 4.
// Financial math implementation for Increment 3. Pure calculation, no files, no floats.
#include "Pricing.h"
#include "Units.h"

BillLine priceLine(const Product& product, Quantity quantity) {
    BillLine line;
    line.acronym = product.acronym;
    line.unitPrice = product.unitPrice;
    line.quantity = quantity;
    line.discount = multiplyPriceByQuantity(product.discountPerUnit, quantity);
    return line;
}

Bill buildBill(const Order& order, const ProductCatalog& catalog, const BillOptions& options,
               const std::vector<std::shared_ptr<IDiscountRule>>& rules) {
    Bill bill;
    bill.id = options.billId;
    bill.customerName = order.customerName.empty() ? "Walk-in" : order.customerName;
    bill.orderId = order.id;
    bill.date = options.date;
    bill.time = options.time;
    bill.status = (order.status == OrderStatus::InProgress) ? OrderStatus::Paid : order.status;
    bill.paymentMode = order.mode;
    bill.merchant = order.merchant;

    Money subtotal = 0;
    Money tax = 0;
    Money itemDiscount = 0;

    for (const auto& orderLine : order.lines) {
        std::optional<Product> prod = catalog.findById(orderLine.productId);
        if (!prod && !orderLine.barcode.empty()) {
            prod = catalog.findByBarcode(orderLine.barcode);
        }
        if (!prod) {
            continue; // Gracefully skip uncatalogued products
        }

        Quantity qty = orderLine.quantity;
        Money lineSub = multiplyPriceByQuantity(prod->unitPrice, qty);
        Money lineTax = multiplyPriceByQuantity(prod->taxPerUnit, qty);
        Money lineDisc = multiplyPriceByQuantity(prod->discountPerUnit, qty);

        BillLine line;
        line.acronym = prod->acronym;
        line.unitPrice = prod->unitPrice;
        line.quantity = qty;
        line.discount = lineDisc;

        bill.lines.push_back(line);

        subtotal += lineSub;
        tax += lineTax;
        itemDiscount += lineDisc;
    }

    Money base = subtotal + tax - itemDiscount;
    if (base < 0) {
        base = 0;
    }

    Money seasonalDiscount = 0;
    Money specialDiscount = 0;

    for (const auto& rule : rules) {
        if (!rule) continue;
        if (rule->name() == "Seasonal") {
            seasonalDiscount += rule->amount(base, options);
        } else if (rule->name() == "Special") {
            specialDiscount += rule->amount(base, options);
        } else {
            // Extensible: any new discount rule applies to the same base
            Money extra = rule->amount(base, options);
            specialDiscount += extra;
        }
    }

    Money totalDiscounts = seasonalDiscount + specialDiscount;
    Money payable = (base > totalDiscounts) ? (base - totalDiscounts) : 0;

    Money received = options.received;
    Money balance = 0;
    if (order.mode == PaymentMode::Cash) {
        if (received >= payable) {
            balance = received - payable;
        }
    } else {
        // Card or digital wallet: received = payable, balance = 0
        received = payable;
        balance = 0;
    }

    bill.subtotal = subtotal;
    bill.tax = tax;
    bill.itemDiscount = itemDiscount;
    bill.seasonalDiscount = seasonalDiscount;
    bill.specialDiscount = specialDiscount;
    bill.payable = payable;
    bill.received = received;
    bill.balance = balance;
    bill.discount = itemDiscount + seasonalDiscount + specialDiscount;

    return bill;
}

Bill buildBill(const Order& order, const ProductCatalog& catalog, const BillOptions& options) {
    std::vector<std::shared_ptr<IDiscountRule>> defaultRules = {
        std::make_shared<SeasonalDiscountRule>(),
        std::make_shared<SpecialDiscountRule>()
    };
    return buildBill(order, catalog, options, defaultRules);
}

Bill buildBill(const Order& order, const ProductCatalog& catalog, const std::string& billId) {
    BillOptions options;
    options.billId = billId;
    options.date = order.date;
    options.time = order.time;
    options.seasonalPercent = 0;
    options.specialPercent = 0;
    options.received = 0;
    return buildBill(order, catalog, options);
}

Money changeDue(Money payable, Money tendered) {
    if (tendered <= payable) {
        return 0;
    }
    return tendered - payable;
}
