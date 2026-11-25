// OWNER: Member 3. Reopens held/in-progress orders and paid orders within the 7-day edit window.
#include "OrderEditService.h"
#include "EditPolicy.h"

namespace {

std::string findBillDateForOrder(const BillStore& bills, const Order& order) {
    std::string foundDate;
    for (const auto& bill : bills.bills()) {
        if (bill.orderId == order.id && !bill.date.empty()) {
            foundDate = bill.date;
        }
    }
    if (!foundDate.empty()) {
        return foundDate;
    }
    return order.date;
}

} // namespace

OperationResult OrderEditService::open(OrderManager& manager,
                                       const OrderStore& orders,
                                       const BillStore& bills,
                                       const std::string& orderId,
                                       const std::string& today,
                                       bool managerApproved) {
    const auto orderOpt = orders.findById(orderId);
    if (!orderOpt.has_value()) {
        return {false, "Order not found: " + orderId};
    }

    const Order& order = *orderOpt;
    switch (order.status) {
        case OrderStatus::Archived:
            return {false, "Archived orders cannot be reopened or edited."};

        case OrderStatus::Paid: {
            if (!managerApproved) {
                return {false, "Manager approval is required to edit a paid order."};
            }
            const std::string billDate = findBillDateForOrder(bills, order);
            if (!EditPolicy::withinEditWindow(billDate, today)) {
                const std::string until = EditPolicy::editWindowUntilDate(billDate);
                if (!until.empty()) {
                    return {false, "Paid order edit window expired (editable until " + until + ")."};
                }
                return {false, "Paid order is outside the 7-day edit window."};
            }
            return manager.beginPaidEdit(order);
        }

        case OrderStatus::OnHold:
            return manager.resume(orderId);

        case OrderStatus::InProgress:
            return manager.openStoredOrder(orderId);
    }

    return {false, "Unsupported order status."};
}

OperationResult OrderEditService::discard(OrderManager& manager) {
    return manager.discardEdit();
}
