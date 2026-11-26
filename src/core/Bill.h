#pragma once
// OWNER: Member 4. Shared contract for Increment 3 billing data structures.
#include <string>
#include <vector>
#include <cstdint>
#include "Order.h"

#ifndef PERCENT_TYPE_DEFINED
#define PERCENT_TYPE_DEFINED
using Percent = std::int64_t; // hundredths of a percent: 8% = 800, 12.50% = 1250
#endif

// Stored in BillDetail.txt (one row per product)
struct BillLine {
    std::string acronym;
    Money unitPrice = 0;       // Base price without tax (paisa)
    Quantity quantity = 0;     // In thousandths (e.g. 2 Count = 2000, 1.250 Weight = 1250)
    Money discount = 0;        // discount = discountPerUnit x quantity (rounded half up)
};

// Stored in Bill.txt (one row per bill)
struct Bill {
    std::string id;                                     // 9 digits, e.g. "000000031"
    std::string customerName;
    std::string orderId;                                // 8 digits, e.g. "00000722"
    Money subtotal = 0;                                 // subtotal = price x qty, no tax
    Money tax = 0;                                      // sum of taxPerUnit x qty
    Money itemDiscount = 0;                             // sum of discountPerUnit x qty
    Money payable = 0;                                  // payable = base - seasonal - special (never < 0)
    Money received = 0;                                 // cash: amount tendered; card/wallet: payable
    Money balance = 0;                                  // cash: received - payable; card/wallet: 0
    OrderStatus status = OrderStatus::Paid;             // Reuses OrderStatus enum from Order.h
    Money seasonalDiscount = 0;                         // seasonal discount amount in paisa
    Money specialDiscount = 0;                          // special discount amount in paisa
    std::string date;                                   // e.g. "2026-11-22"
    std::string time;                                   // e.g. "14:05:09"
    std::vector<BillLine> lines;                        // stored in BillDetail.txt

    // Convenience fields and aliases for backward compatibility
    PaymentMode paymentMode = PaymentMode::Cash;
    std::string merchant;
    Money discount = 0;                                 // itemDiscount + seasonalDiscount + specialDiscount
};

// Options for building a bill
struct BillOptions {
    std::string billId;
    std::string date;
    std::string time;
    Percent seasonalPercent = 0;
    Percent specialPercent = 0;
    Money received = 0;
};

// Payment information supplied by cashier during checkout.
struct PaymentInfo {
    PaymentMode mode = PaymentMode::Cash;
    std::string merchant;                               // Required for Card/Wallet; empty for Cash
    Money received = 0;                                 // Cash tendered
    Percent specialPercent = 0;                         // Special discount percent (0 to 800)
    bool specialApproved = false;                       // Whether manager approved special discount
};
