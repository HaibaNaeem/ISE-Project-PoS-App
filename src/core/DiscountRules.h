#pragma once
// OWNER: Member 4. Discount rules hierarchy (OCP: Open-Closed Principle).
#include <string>
#include <vector>
#include <memory>
#include "CommonTypes.h"
#include "Bill.h"
#include "Units.h"

// Private copy of percentOf until Member 2 merges Units percent functions
namespace BillingMath {
inline Money percentOf(Money base, Percent percent) {
    if (base <= 0 || percent <= 0) return 0;
    return (base * percent + 5000) / 10000; // rounded half up, integers only
}
}

class IDiscountRule {
public:
    virtual ~IDiscountRule() = default;
    virtual std::string name() const = 0;
    virtual Money amount(Money base, const BillOptions& options) const = 0;
    virtual OperationResult validate(const BillOptions& options) const {
        return {true, "Valid discount."};
    }
};

class SeasonalDiscountRule : public IDiscountRule {
public:
    std::string name() const override { return "Seasonal"; }

    OperationResult validate(const BillOptions& options) const override {
        if (options.seasonalPercent < 0) {
            return {false, "Seasonal discount percentage cannot be negative."};
        }
        if (options.seasonalPercent > 10000) { // > 100.00%
            return {false, "Seasonal discount percentage cannot exceed 100.00%."};
        }
        return {true, "Seasonal discount valid."};
    }

    Money amount(Money base, const BillOptions& options) const override {
        if (base <= 0 || options.seasonalPercent <= 0) return 0;
        return BillingMath::percentOf(base, options.seasonalPercent);
    }
};

class SpecialDiscountRule : public IDiscountRule {
public:
    static constexpr Percent MAX_SPECIAL_PERCENT = 800; // 8.00%

    std::string name() const override { return "Special"; }

    OperationResult validate(const BillOptions& options) const override {
        return validatePercent(options.specialPercent);
    }

    static OperationResult validatePercent(Percent percent) {
        if (percent < 0) {
            return {false, "Special discount percentage cannot be negative."};
        }
        if (percent > MAX_SPECIAL_PERCENT) {
            return {false, "Special discount percentage cannot exceed 8.00% (max 800). Special above 8.00% is refused."};
        }
        return {true, "Special discount valid."};
    }

    Money amount(Money base, const BillOptions& options) const override {
        if (base <= 0 || options.specialPercent <= 0 || options.specialPercent > MAX_SPECIAL_PERCENT) {
            return 0;
        }
        return BillingMath::percentOf(base, options.specialPercent);
    }
};
