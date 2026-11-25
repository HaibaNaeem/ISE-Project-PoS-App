#pragma once
// OWNER: Member 4.
// Increment 3: Orchestrates atomic all-or-nothing checkout and edit-paid-order operations.
// Commits Product.txt, Order.txt, OrderDetail.txt, Bill.txt, and BillDetail.txt together via FileTransaction.
#include <string>
#include <memory>
#include "CommonTypes.h"
#include "Bill.h"
#include "BillStore.h"
#include "OrderManager.h"
#include "OrderStore.h"
#include "ProductCatalog.h"
#include "SeasonService.h"

class CheckoutService {
public:
    // Injected dependencies: catalog, order store, bill store, product file path, and season service.
    CheckoutService(ProductCatalog& catalog, OrderStore& orders, BillStore& bills,
                    std::string productPath, const SeasonService& seasons);

    // Backward-compatible constructor (defaults to empty SeasonService)
    CheckoutService(ProductCatalog& catalog, OrderStore& orders, BillStore& bills,
                    std::string productPath);

    // Pay for a new order:
    // 1) Re-check stock for every line (StockRules::hasEnoughStock) - stock never below zero.
    // 2) seasonalPercent = SeasonService::percentFor(todayText()).
    // 3) Validate payment (cash: received >= payable; card/wallet: merchant required, received == payable;
    //    special discount > 0 requires approval, percent <= 800).
    // 4) buildBill with BillStore::nextBillId(), today's date and now time.
    // 5) Work on COPIES: stock down, order paid, bill added.
    // 6) Write Product.txt, Order.txt, OrderDetail.txt, Bill.txt, BillDetail.txt together (FileTransaction).
    // 7) Only upon success: update in-memory state and clear current order.
    OperationResult pay(OrderManager& manager, const PaymentInfo& payment, Bill& out);

    // Save changes to an edited paid order:
    // - Stock changes by the DIFFERENCE per product (new qty - old qty), checked against available (+ old qty).
    // - SAME Bill ID, bill date and bill time as the original bill.
    // - Season percent is looked up for the ORIGINAL bill date.
    // - Amount received typed again; special discount entered again (needs approval if applicable).
    // - Bill and BillDetail rows are REPLACED (BillStore::replace), Order/OrderDetail/Product updated.
    // - All five files written together via FileTransaction; all-or-nothing.
    OperationResult saveEdited(OrderManager& manager, const PaymentInfo& payment, Bill& out);

    // For testing: force FileTransaction failure to verify all-or-nothing rollback.
    void simulateWriteFailure(bool enable) { simulateFailure_ = enable; }

private:
    ProductCatalog* catalog_;
    OrderStore* orders_;
    BillStore* bills_;
    std::string productPath_;
    const SeasonService* seasons_;
    std::unique_ptr<SeasonService> defaultSeasons_;
    bool simulateFailure_ = false;
};
