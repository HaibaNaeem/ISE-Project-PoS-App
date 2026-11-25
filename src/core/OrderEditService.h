#pragma once
// OWNER: Member 3. Reopens held/in-progress orders and paid orders within the 7-day edit window.
#include <string>
#include "BillStore.h"
#include "CommonTypes.h"
#include "OrderManager.h"
#include "OrderStore.h"

class OrderEditService {
public:
    // Opens an order into manager:
    // - archived: always refused
    // - paid: requires managerApproved && withinEditWindow(billDate, today); opens in-memory copy with original() set
    // - on-hold / in-progress: opens like resume()
    static OperationResult open(OrderManager& manager,
                                const OrderStore& orders,
                                const BillStore& bills,
                                const std::string& orderId,
                                const std::string& today,
                                bool managerApproved);

    // Discards an active in-memory paid-order edit without modifying disk or OrderStore.
    static OperationResult discard(OrderManager& manager);
};
