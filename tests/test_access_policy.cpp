// OWNER: Member 1. Exhaustive tests for AccessPolicy (every Role x Action).
#include "AccessPolicy.h"
#include "TestHelpers.h"
#include <vector>

namespace {

bool expectedAllowed(Role role, Action action) {
    if (role == Role::Manager) return true;
    switch (action) {
        case Action::RecordOrder:
        case Action::OpenOrder:
        case Action::PrintBill:
        case Action::ViewProduct:
            return true;
        default:
            return false;
    }
}

bool expectedApproval(Role role, Action action) {
    if (role != Role::Cashier) return false;
    return action == Action::EditPaidOrder || action == Action::ApplySpecialDiscount;
}

} // namespace

int main() {
    const std::vector<Role> roles = {Role::Cashier, Role::Manager};
    const std::vector<Action> actions = {
        Action::RecordOrder,
        Action::OpenOrder,
        Action::EditPaidOrder,
        Action::PrintBill,
        Action::ViewProduct,
        Action::ManageProducts,
        Action::ManageCategories,
        Action::ViewInventory,
        Action::UpdateInventory,
        Action::ManageSeasons,
        Action::ViewReports,
        Action::ApplySpecialDiscount
    };

    for (Role r : roles) {
        for (Action a : actions) {
            CHECK_EQ(isAllowed(r, a), expectedAllowed(r, a));
            CHECK_EQ(AccessPolicy::isAllowed(r, a), expectedAllowed(r, a));
            CHECK_EQ(needsManagerApproval(r, a), expectedApproval(r, a));
            CHECK_EQ(AccessPolicy::needsManagerApproval(r, a), expectedApproval(r, a));
        }
    }

    // Explicit spot checks for clarity
    CHECK(isAllowed(Role::Cashier, Action::RecordOrder));
    CHECK(isAllowed(Role::Cashier, Action::OpenOrder));
    CHECK(isAllowed(Role::Cashier, Action::PrintBill));
    CHECK(isAllowed(Role::Cashier, Action::ViewProduct));
    CHECK(!isAllowed(Role::Cashier, Action::EditPaidOrder));
    CHECK(!isAllowed(Role::Cashier, Action::ManageProducts));
    CHECK(!isAllowed(Role::Cashier, Action::ManageCategories));
    CHECK(!isAllowed(Role::Cashier, Action::ViewInventory));
    CHECK(!isAllowed(Role::Cashier, Action::UpdateInventory));
    CHECK(!isAllowed(Role::Cashier, Action::ManageSeasons));
    CHECK(!isAllowed(Role::Cashier, Action::ViewReports));
    CHECK(!isAllowed(Role::Cashier, Action::ApplySpecialDiscount));

    CHECK(needsManagerApproval(Role::Cashier, Action::EditPaidOrder));
    CHECK(needsManagerApproval(Role::Cashier, Action::ApplySpecialDiscount));
    CHECK(!needsManagerApproval(Role::Cashier, Action::RecordOrder));
    CHECK(!needsManagerApproval(Role::Cashier, Action::ManageProducts));

    CHECK(isAllowed(Role::Manager, Action::EditPaidOrder));
    CHECK(isAllowed(Role::Manager, Action::ApplySpecialDiscount));
    CHECK(!needsManagerApproval(Role::Manager, Action::EditPaidOrder));
    CHECK(!needsManagerApproval(Role::Manager, Action::ApplySpecialDiscount));

    return testSummary("test_access_policy");
}
