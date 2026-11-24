// OWNER: Member 1. The ONLY place that decides who may do what (no Qt).
#include "AccessPolicy.h"

namespace AccessPolicy {

bool isAllowed(Role role, Action action) {
    if (role == Role::Manager) {
        return true;
    }
    if (role == Role::Cashier) {
        switch (action) {
            case Action::RecordOrder:
            case Action::OpenOrder:
            case Action::PrintBill:
            case Action::ViewProduct:
                return true;
            case Action::EditPaidOrder:
            case Action::ManageProducts:
            case Action::ManageCategories:
            case Action::ViewInventory:
            case Action::UpdateInventory:
            case Action::ManageSeasons:
            case Action::ViewReports:
            case Action::ApplySpecialDiscount:
                return false;
        }
    }
    return false;
}

bool needsManagerApproval(Role role, Action action) {
    if (role == Role::Cashier) {
        return action == Action::EditPaidOrder ||
               action == Action::ApplySpecialDiscount;
    }
    return false;
}

} // namespace AccessPolicy
