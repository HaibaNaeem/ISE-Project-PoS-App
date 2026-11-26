#pragma once
// OWNER: Member 4. Increment 3 dual-file persistence (Bill.txt + BillDetail.txt).
#include <string>
#include <vector>
#include <optional>
#include "CommonTypes.h"
#include "Bill.h"

class BillStore {
public:
    BillStore() = default;
    explicit BillStore(std::string billPath, std::string detailPath = "");

    // Load bills and bill details from the given paths.
    // Missing files load as empty without crashing.
    // Malformed rows and old Increment 2 format rows are skipped with clear warnings including line numbers.
    OperationResult load(const std::string& billPath, const std::string& detailPath);
    OperationResult load();

    // Save bills and bill details to the given paths.
    OperationResult save(const std::string& billPath, const std::string& detailPath) const;
    OperationResult save() const;

    // Produces the entire Bill.txt and BillDetail.txt content as text
    std::string serializeBills() const;
    std::string serializeDetails() const;

    // Backward-compatibility alias: produces Bill.txt content
    std::string serialize() const { return serializeBills(); }

    // Adds a new bill. Bill ID must be unique.
    OperationResult add(const Bill& bill);

    // Replaces an existing bill (used when editing a paid order).
    OperationResult replace(const Bill& bill);

    // Backward-compatibility alias
    void addBill(const Bill& bill) { add(bill); }

    // Lookup functions
    std::optional<Bill> findById(const std::string& id) const;
    std::optional<Bill> findByOrderId(const std::string& orderId) const;
    std::vector<Bill> all() const { return bills_; }

    // Generates the next 9-digit zero-padded Bill ID (e.g. "000000001", "000000032").
    std::string nextBillId() const;

    // Clears all in-memory bills.
    void clear();

    const std::string& billPath() const { return billPath_; }
    const std::string& detailPath() const { return detailPath_; }
    const std::string& path() const { return billPath_; } // Backward compatibility
    void setPaths(std::string billPath, std::string detailPath);
    void setPath(const std::string& path) { billPath_ = path; }

    std::vector<Bill>& bills() { return bills_; }
    const std::vector<Bill>& bills() const { return bills_; }

private:
    std::string billPath_;
    std::string detailPath_;
    std::vector<Bill> bills_;
};
