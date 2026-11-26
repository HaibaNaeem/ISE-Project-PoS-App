// OWNER: Member 4.
// Atomic payment orchestration implementation for Increment 3.
#include "CheckoutService.h"
#include "Pricing.h"
#include "StockRules.h"
#include "RecordFormat.h"
#include "Units.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <ctime>

namespace fs = std::filesystem;

namespace {

// Private date/time helpers until Member 2 merges DateText
namespace BillingDates {
inline std::string todayText() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}
inline std::string nowTimeText() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return std::string(buf);
}
}

// Executes an atomic commit of all 5 files with full rollback capability
OperationResult commitAllOrNothing(
    const std::vector<std::pair<std::string, std::string>>& fileEntries,
    bool simulateFailure)
{
    // Try calling M2's FileTransaction first
    FileTransaction ft;
    ft.simulateFailure(simulateFailure);
    for (const auto& [target, content] : fileEntries) {
        if (!target.empty()) {
            ft.add(target, content);
        }
    }
    OperationResult ftRes = ft.commit();
    if (ftRes.message.find("TODO") == std::string::npos) {
        // M2's real FileTransaction ran
        return ftRes;
    }

    // Fallback transaction if M2's commit() is still a stub in dev branch
    std::error_code ec;
    auto cleanupTmp = [&] {
        for (const auto& [target, _] : fileEntries) {
            if (!target.empty()) fs::remove(target + ".tmp", ec);
        }
    };

    // Step 1: Write all content to temporary files
    for (const auto& [target, content] : fileEntries) {
        if (target.empty()) continue;
        std::ofstream out(target + ".tmp", std::ios::binary);
        if (!out.is_open()) {
            cleanupTmp();
            return {false, "Failed to create temp file: " + target + ".tmp"};
        }
        out << content;
        out.close();
        if (!out) {
            cleanupTmp();
            return {false, "Failed to write temp file: " + target + ".tmp"};
        }
    }

    // Simulated failure hook for testing all-or-nothing rollback
    if (simulateFailure) {
        cleanupTmp();
        return {false, "Transaction failed (simulated). No file was modified."};
    }

    // Step 2: Swap files atomically via backup rename
    size_t replacedCount = 0;
    bool success = true;
    for (size_t i = 0; i < fileEntries.size(); ++i) {
        const std::string& target = fileEntries[i].first;
        if (target.empty()) continue;

        if (fs::exists(target)) {
            fs::rename(target, target + ".bak", ec);
            if (ec) {
                success = false;
                break;
            }
        }

        fs::rename(target + ".tmp", target, ec);
        if (ec) {
            success = false;
            break;
        }
        ++replacedCount;
    }

    if (!success) {
        // Rollback any replaced files
        for (size_t i = 0; i <= replacedCount && i < fileEntries.size(); ++i) {
            const std::string& target = fileEntries[i].first;
            if (target.empty()) continue;
            if (fs::exists(target + ".bak")) {
                fs::remove(target, ec);
                fs::rename(target + ".bak", target, ec);
            }
        }
        cleanupTmp();
        return {false, "Transaction failed during file replacement. Rollback completed."};
    }

    // Clean up backup files on success
    for (const auto& [target, _] : fileEntries) {
        if (!target.empty()) fs::remove(target + ".bak", ec);
    }

    return {true, "Files committed successfully."};
}

} // anonymous namespace

CheckoutService::CheckoutService(
    ProductCatalog& catalog,
    OrderStore& orders,
    BillStore& bills,
    std::string productPath,
    const SeasonService& seasons)
    : catalog_(&catalog), orders_(&orders), bills_(&bills),
      productPath_(std::move(productPath)), seasons_(&seasons) {}

CheckoutService::CheckoutService(
    ProductCatalog& catalog,
    OrderStore& orders,
    BillStore& bills,
    std::string productPath)
    : catalog_(&catalog), orders_(&orders), bills_(&bills),
      productPath_(std::move(productPath)) {
    defaultSeasons_ = std::make_unique<SeasonService>();
    seasons_ = defaultSeasons_.get();
}

OperationResult CheckoutService::pay(OrderManager& manager, const PaymentInfo& payment, Bill& out) {
    // -------------------------------------------------------------
    // Step 1: Validate active order
    // -------------------------------------------------------------
    if (!manager.current().has_value()) {
        return {false, "No active order to checkout."};
    }

    const Order& currentOrder = *manager.current();
    if (currentOrder.lines.empty()) {
        return {false, "Cannot checkout an empty order."};
    }

    if (payment.mode == PaymentMode::None) {
        return {false, "Please select a valid payment mode (Cash, Card, or Wallet)."};
    }

    // -------------------------------------------------------------
    // Step 2: Validate special discount parameters
    // -------------------------------------------------------------
    if (payment.specialPercent < 0) {
        return {false, "Special discount percentage cannot be negative."};
    }
    if (payment.specialPercent > 800) {
        return {false, "Special discount percentage cannot exceed 8.00% (max 800). Special above 8.00% is refused."};
    }
    if (payment.specialPercent > 0 && !payment.specialApproved) {
        return {false, "Special discount requires manager approval."};
    }

    // -------------------------------------------------------------
    // Step 3: Re-check stock for every product using StockRules
    // -------------------------------------------------------------
    std::map<std::string, Quantity> orderQuantities;
    for (const auto& line : currentOrder.lines) {
        orderQuantities[line.productId] += line.quantity;
    }

    for (const auto& [pid, requestedQty] : orderQuantities) {
        std::optional<Product> prod = catalog_->findById(pid);
        if (!prod) {
            return {false, "Product not found in catalog: " + pid};
        }
        if (!hasEnoughStock(0, requestedQty, prod->available)) {
            return {false, "Insufficient stock for product '" + prod->name + "'."};
        }
    }

    // -------------------------------------------------------------
    // Step 4: Look up seasonal discount for today
    // -------------------------------------------------------------
    std::string dateStr = currentOrder.date.empty() ? BillingDates::todayText() : currentOrder.date;
    std::string timeStr = currentOrder.time.empty() ? BillingDates::nowTimeText() : currentOrder.time;
    Percent seasonalPercent = seasons_->percentFor(dateStr);

    // -------------------------------------------------------------
    // Step 5: Build final Bill
    // -------------------------------------------------------------
    std::string nextId = bills_->nextBillId();
    BillOptions options;
    options.billId = nextId;
    options.date = dateStr;
    options.time = timeStr;
    options.seasonalPercent = seasonalPercent;
    options.specialPercent = payment.specialPercent;
    options.received = payment.received;

    Bill finalBill = buildBill(currentOrder, *catalog_, options);
    if (finalBill.lines.empty()) {
        return {false, "Failed to price order lines."};
    }

    // -------------------------------------------------------------
    // Step 6: Validate payment details against payable amount
    // -------------------------------------------------------------
    if (payment.mode == PaymentMode::Cash) {
        Money received = payment.received;
        if (received == 0) {
            received = finalBill.payable;
        } else if (received < finalBill.payable) {
            return {false, "Insufficient cash received. Received: " + formatMoney(received) +
                           " PKR, Payable: " + formatMoney(finalBill.payable) + " PKR."};
        }
        finalBill.received = received;
        finalBill.balance = received - finalBill.payable;
    } else {
        // Card or Wallet
        if (payment.merchant.empty()) {
            return {false, "Merchant name is required for Card and Wallet payments."};
        }
        finalBill.received = finalBill.payable;
        finalBill.balance = 0;
    }

    finalBill.status = OrderStatus::Paid;
    finalBill.paymentMode = payment.mode;
    finalBill.merchant = payment.merchant;

    // -------------------------------------------------------------
    // Step 7: Work on COPIES ONLY (All-or-Nothing preparation)
    // -------------------------------------------------------------
    // Copy of Catalog with stock reduced
    ProductCatalog catalogCopy = *catalog_;
    for (const auto& [pid, qty] : orderQuantities) {
        OperationResult reduceRes = catalogCopy.reduceStock(pid, qty);
        if (!reduceRes.success) {
            return {false, "Failed to deduct stock for product ID " + pid};
        }
    }

    // Copy of Order updated to Paid status
    Order paidOrder = currentOrder;
    paidOrder.status = OrderStatus::Paid;
    paidOrder.mode = payment.mode;
    paidOrder.merchant = payment.merchant;
    paidOrder.paymentAmount = finalBill.payable;
    paidOrder.date = dateStr;
    paidOrder.time = timeStr;

    OrderStore ordersCopy = *orders_;
    bool orderUpdated = false;
    for (auto& ord : ordersCopy.orders()) {
        if (ord.id == paidOrder.id) {
            ord = paidOrder;
            orderUpdated = true;
            break;
        }
    }
    if (!orderUpdated) {
        ordersCopy.orders().push_back(paidOrder);
    }

    // Copy of BillStore with new bill added
    BillStore billsCopy = *bills_;
    OperationResult addRes = billsCopy.add(finalBill);
    if (!addRes.success) {
        return {false, "Failed to add bill: " + addRes.message};
    }

    // -------------------------------------------------------------
    // Step 8: Write all five files atomically via FileTransaction
    // -------------------------------------------------------------
    std::string productContent    = catalogCopy.serialize();
    std::string orderContent      = ordersCopy.serializeOrders();
    std::string detailContent     = ordersCopy.serializeDetails();
    std::string billContent       = billsCopy.serializeBills();
    std::string billDetailContent = billsCopy.serializeDetails();

    std::vector<std::pair<std::string, std::string>> filesToCommit;
    if (!productPath_.empty()) {
        filesToCommit.push_back({productPath_, productContent});
    }
    if (!orders_->orderPath().empty()) {
        filesToCommit.push_back({orders_->orderPath(), orderContent});
    }
    if (!orders_->detailPath().empty()) {
        filesToCommit.push_back({orders_->detailPath(), detailContent});
    }
    if (!bills_->billPath().empty()) {
        filesToCommit.push_back({bills_->billPath(), billContent});
    }
    if (!bills_->detailPath().empty()) {
        filesToCommit.push_back({bills_->detailPath(), billDetailContent});
    }

    OperationResult commitRes = commitAllOrNothing(filesToCommit, simulateFailure_);
    if (!commitRes.success) {
        // Rollback: files, catalog, orders, and manager remain completely unchanged
        return {false, "Checkout failed: " + commitRes.message};
    }

    // -------------------------------------------------------------
    // Step 9: ONLY after transaction succeeds, update in-memory state
    // -------------------------------------------------------------
    *catalog_ = std::move(catalogCopy);
    *orders_  = std::move(ordersCopy);
    *bills_   = std::move(billsCopy);

    // Clear active order in manager
    manager.clearCurrent();

    // Populate output bill
    out = finalBill;

    return {true, "Payment successful.", finalBill.id};
}

OperationResult CheckoutService::saveEdited(OrderManager& manager, const PaymentInfo& payment, Bill& out) {
    // -------------------------------------------------------------
    // Step 1: Validate active edited order
    // -------------------------------------------------------------
    if (!manager.current().has_value()) {
        return {false, "No active order to save."};
    }

    const Order& currentOrder = *manager.current();
    if (currentOrder.lines.empty()) {
        return {false, "Cannot save an empty order."};
    }

    // Find the original saved order in OrderStore
    const Order* origOrder = nullptr;
    for (const auto& ord : orders_->orders()) {
        if (ord.id == currentOrder.id) {
            origOrder = &ord;
            break;
        }
    }
    if (!origOrder) {
        return {false, "Original order not found in store for order ID: " + currentOrder.id};
    }

    // Find the original bill in BillStore
    std::optional<Bill> origBill = bills_->findByOrderId(currentOrder.id);
    if (!origBill) {
        origBill = bills_->findById(currentOrder.id);
    }
    if (!origBill) {
        return {false, "Original bill not found for order ID: " + currentOrder.id};
    }

    // -------------------------------------------------------------
    // Step 2: Validate special discount parameters
    // -------------------------------------------------------------
    if (payment.specialPercent < 0) {
        return {false, "Special discount percentage cannot be negative."};
    }
    if (payment.specialPercent > 800) {
        return {false, "Special discount percentage cannot exceed 8.00% (max 800). Special above 8.00% is refused."};
    }
    if (payment.specialPercent > 0 && !payment.specialApproved) {
        return {false, "Special discount requires manager approval."};
    }

    // -------------------------------------------------------------
    // Step 3: Stock check by DIFFERENCE per product:
    // available + quantity that line had before
    // -------------------------------------------------------------
    std::map<std::string, Quantity> oldQuantities;
    for (const auto& line : origOrder->lines) {
        oldQuantities[line.productId] += line.quantity;
    }

    std::map<std::string, Quantity> newQuantities;
    for (const auto& line : currentOrder.lines) {
        newQuantities[line.productId] += line.quantity;
    }

    std::set<std::string> allProductIds;
    for (const auto& [pid, _] : oldQuantities) allProductIds.insert(pid);
    for (const auto& [pid, _] : newQuantities) allProductIds.insert(pid);

    for (const auto& pid : allProductIds) {
        Quantity oldQty = oldQuantities.count(pid) ? oldQuantities[pid] : 0;
        Quantity newQty = newQuantities.count(pid) ? newQuantities[pid] : 0;
        Quantity diff = newQty - oldQty;

        auto prod = catalog_->findById(pid);
        if (!prod) {
            return {false, "Product not found in catalog: " + pid};
        }

        // Check if available stock can cover any positive difference
        if (diff > 0 && prod->available < diff) {
            return {false, "Insufficient stock for product '" + prod->name + "'."};
        }
    }

    // -------------------------------------------------------------
    // Step 4: Look up seasonal discount for the ORIGINAL bill date
    // -------------------------------------------------------------
    Percent seasonalPercent = seasons_->percentFor(origBill->date);

    // -------------------------------------------------------------
    // Step 5: Build rewritten Bill preserving ID, date and time
    // -------------------------------------------------------------
    BillOptions options;
    options.billId = origBill->id;
    options.date = origBill->date;
    options.time = origBill->time;
    options.seasonalPercent = seasonalPercent;
    options.specialPercent = payment.specialPercent;
    options.received = payment.received;

    Bill editedBill = buildBill(currentOrder, *catalog_, options);
    if (editedBill.lines.empty()) {
        return {false, "Failed to price edited order lines."};
    }

    // Validate payment details
    if (payment.mode == PaymentMode::Cash) {
        Money received = payment.received;
        if (received == 0) {
            received = editedBill.payable;
        } else if (received < editedBill.payable) {
            return {false, "Insufficient cash received. Received: " + formatMoney(received) +
                           " PKR, Payable: " + formatMoney(editedBill.payable) + " PKR."};
        }
        editedBill.received = received;
        editedBill.balance = received - editedBill.payable;
    } else {
        if (payment.merchant.empty()) {
            return {false, "Merchant name is required for Card and Wallet payments."};
        }
        editedBill.received = editedBill.payable;
        editedBill.balance = 0;
    }

    editedBill.status = OrderStatus::Paid;
    editedBill.paymentMode = payment.mode;
    editedBill.merchant = payment.merchant;

    // -------------------------------------------------------------
    // Step 6: Work on COPIES ONLY (All-or-Nothing preparation)
    // -------------------------------------------------------------
    // Adjust catalog stock by difference
    std::vector<Product> prods = catalog_->all();
    for (auto& p : prods) {
        Quantity oldQty = oldQuantities.count(p.id) ? oldQuantities[p.id] : 0;
        Quantity newQty = newQuantities.count(p.id) ? newQuantities[p.id] : 0;
        Quantity diff = newQty - oldQty;
        p.available -= diff;
    }
    ProductCatalog catalogCopy(std::move(prods));

    // Update order in OrderStore copy
    Order updatedOrder = currentOrder;
    updatedOrder.status = OrderStatus::Paid;
    updatedOrder.mode = payment.mode;
    updatedOrder.merchant = payment.merchant;
    updatedOrder.paymentAmount = editedBill.payable;
    updatedOrder.date = origOrder->date;
    updatedOrder.time = origOrder->time;

    OrderStore ordersCopy = *orders_;
    bool orderUpdated = false;
    for (auto& ord : ordersCopy.orders()) {
        if (ord.id == updatedOrder.id) {
            ord = updatedOrder;
            orderUpdated = true;
            break;
        }
    }
    if (!orderUpdated) {
        ordersCopy.orders().push_back(updatedOrder);
    }

    // Replace bill in BillStore copy
    BillStore billsCopy = *bills_;
    OperationResult repRes = billsCopy.replace(editedBill);
    if (!repRes.success) {
        return {false, "Failed to replace bill: " + repRes.message};
    }

    // -------------------------------------------------------------
    // Step 7: Write all five files atomically via FileTransaction
    // -------------------------------------------------------------
    std::string productContent    = catalogCopy.serialize();
    std::string orderContent      = ordersCopy.serializeOrders();
    std::string detailContent     = ordersCopy.serializeDetails();
    std::string billContent       = billsCopy.serializeBills();
    std::string billDetailContent = billsCopy.serializeDetails();

    std::vector<std::pair<std::string, std::string>> filesToCommit;
    if (!productPath_.empty()) {
        filesToCommit.push_back({productPath_, productContent});
    }
    if (!orders_->orderPath().empty()) {
        filesToCommit.push_back({orders_->orderPath(), orderContent});
    }
    if (!orders_->detailPath().empty()) {
        filesToCommit.push_back({orders_->detailPath(), detailContent});
    }
    if (!bills_->billPath().empty()) {
        filesToCommit.push_back({bills_->billPath(), billContent});
    }
    if (!bills_->detailPath().empty()) {
        filesToCommit.push_back({bills_->detailPath(), billDetailContent});
    }

    OperationResult commitRes = commitAllOrNothing(filesToCommit, simulateFailure_);
    if (!commitRes.success) {
        return {false, "Saving edited order failed: " + commitRes.message};
    }

    // -------------------------------------------------------------
    // Step 8: ONLY upon success, update in-memory state
    // -------------------------------------------------------------
    *catalog_ = std::move(catalogCopy);
    *orders_  = std::move(ordersCopy);
    *bills_   = std::move(billsCopy);

    manager.clearCurrent();
    out = editedBill;

    return {true, "Edited order saved successfully.", editedBill.id};
}
