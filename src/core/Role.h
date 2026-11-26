#pragma once
// OWNER: Member 1. Shared contract: roles and actions for AccessPolicy.

enum class Role {
    Cashier,
    Manager
};

enum class Action {
    RecordOrder,
    OpenOrder,
    EditPaidOrder,
    PrintBill,
    ViewProduct,
    ManageProducts,
    ManageCategories,
    ViewInventory,
    UpdateInventory,
    ManageSeasons,
    ViewReports,
    ApplySpecialDiscount
};
