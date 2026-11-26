#pragma once
// OWNER: Member 1. The ONLY place that decides who may do what (no Qt).
#include "Role.h"

namespace AccessPolicy {

// Returns true if the role may perform the action on its own.
bool isAllowed(Role role, Action action);

// Returns true only when a Cashier needs a Manager's PIN approval to perform the action.
bool needsManagerApproval(Role role, Action action);

} // namespace AccessPolicy

using AccessPolicy::isAllowed;
using AccessPolicy::needsManagerApproval;
