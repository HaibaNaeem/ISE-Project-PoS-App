// OWNER: Member 4.
// Increment 3 dual-file persistence implementation for Bill.txt and BillDetail.txt.
#include "BillStore.h"
#include "RecordFormat.h"
#include "Units.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

namespace {

std::string padLeft(std::int64_t v, int width) {
    std::string s = std::to_string(v);
    while (static_cast<int>(s.size()) < width) {
        s = "0" + s;
    }
    return s;
}

// Helper to format quantity: whole number as integer, fractions up to 3 decimals
std::string formatQty(Quantity qty) {
    if (qty % 1000 == 0) {
        return std::to_string(qty / 1000);
    }
    return formatQuantity(qty, MeasureUnit::Weight);
}

// Helper to parse quantity robustly (supports whole numbers and decimals)
bool parseQtyAny(const std::string& text, Quantity& out) {
    std::string trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
    if (trimmed.empty()) return false;

    // If text has dot, parse as weight (up to 3 decimals)
    if (trimmed.find('.') != std::string::npos) {
        OperationResult r = parseQuantity(trimmed, MeasureUnit::Weight, out);
        return r.success;
    }
    // Else parse as count (whole number)
    OperationResult r = parseQuantity(trimmed, MeasureUnit::Count, out);
    return r.success;
}

} // anonymous namespace

BillStore::BillStore(std::string billPath, std::string detailPath)
    : billPath_(std::move(billPath)), detailPath_(std::move(detailPath)) {}

void BillStore::setPaths(std::string billPath, std::string detailPath) {
    billPath_ = std::move(billPath);
    detailPath_ = std::move(detailPath);
}

void BillStore::clear() {
    bills_.clear();
}

OperationResult BillStore::load(const std::string& billPath, const std::string& detailPath) {
    setPaths(billPath, detailPath);
    return load();
}

OperationResult BillStore::load() {
    bills_.clear();
    std::string warnings;

    // Step 1: Load Bill.txt (one row per bill)
    if (!billPath_.empty() && fs::exists(billPath_)) {
        std::ifstream in(billPath_, std::ios::binary);
        if (!in.is_open()) {
            return {false, "Cannot open bill file: " + billPath_};
        }

        std::string line;
        int lineNum = 0;
        while (std::getline(in, line)) {
            ++lineNum;
            // Trim whitespace
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
            if (trimmed.empty()) continue;

            std::vector<std::string> fields;
            OperationResult splitRes = splitRecord(line, lineNum, fields);
            if (!splitRes.success) {
                warnings += "Bill.txt line " + std::to_string(lineNum) + ": " + splitRes.message + ". Skipped.\n";
                continue;
            }

            // In Increment 3, Bill.txt has 14 fields:
            // billId "customerName" orderId subtotal tax itemDiscount payable received balance status seasonalDiscount specialDiscount date time #
            if (fields.size() != 14) {
                warnings += "Bill.txt line " + std::to_string(lineNum) + ": Invalid field count (" +
                            std::to_string(fields.size()) + ", expected 14). Old Increment 2 format or malformed row skipped.\n";
                continue;
            }

            Bill b;
            b.id = fields[0];
            b.customerName = fields[1]; // splitRecord already removes outer quotes
            b.orderId = fields[2];

            if (!parseMoney(fields[3], b.subtotal).success ||
                !parseMoney(fields[4], b.tax).success ||
                !parseMoney(fields[5], b.itemDiscount).success ||
                !parseMoney(fields[6], b.payable).success ||
                !parseMoney(fields[7], b.received).success ||
                !parseMoney(fields[8], b.balance).success) {
                warnings += "Bill.txt line " + std::to_string(lineNum) + ": Invalid financial amount. Skipped.\n";
                continue;
            }

            if (!parseOrderStatus(fields[9], b.status)) {
                warnings += "Bill.txt line " + std::to_string(lineNum) + ": Invalid order status '" + fields[9] + "'. Skipped.\n";
                continue;
            }

            if (!parseMoney(fields[10], b.seasonalDiscount).success ||
                !parseMoney(fields[11], b.specialDiscount).success) {
                warnings += "Bill.txt line " + std::to_string(lineNum) + ": Invalid discount amount. Skipped.\n";
                continue;
            }

            b.date = fields[12];
            b.time = fields[13];
            b.discount = b.itemDiscount + b.seasonalDiscount + b.specialDiscount;

            // Check uniqueness of Bill ID
            if (findById(b.id).has_value()) {
                warnings += "Bill.txt line " + std::to_string(lineNum) + ": Duplicate bill ID '" + b.id + "'. Skipped.\n";
                continue;
            }

            bills_.push_back(b);
        }
    }

    // Step 2: Load BillDetail.txt (one row per product)
    if (!detailPath_.empty() && fs::exists(detailPath_)) {
        std::ifstream in(detailPath_, std::ios::binary);
        if (!in.is_open()) {
            return {false, "Cannot open bill detail file: " + detailPath_};
        }

        std::string line;
        int lineNum = 0;
        while (std::getline(in, line)) {
            ++lineNum;
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
            if (trimmed.empty()) continue;

            std::vector<std::string> fields;
            OperationResult splitRes = splitRecord(line, lineNum, fields);
            if (!splitRes.success) {
                warnings += "BillDetail.txt line " + std::to_string(lineNum) + ": " + splitRes.message + ". Skipped.\n";
                continue;
            }

            // Expected: billId acronym unitPrice quantity discount # (5 fields)
            if (fields.size() != 5) {
                warnings += "BillDetail.txt line " + std::to_string(lineNum) + ": Invalid field count (" +
                            std::to_string(fields.size()) + ", expected 5). Skipped.\n";
                continue;
            }

            std::string billId = fields[0];
            BillLine bl;
            bl.acronym = fields[1];

            if (!parseMoney(fields[2], bl.unitPrice).success) {
                warnings += "BillDetail.txt line " + std::to_string(lineNum) + ": Invalid unit price '" + fields[2] + "'. Skipped.\n";
                continue;
            }

            if (!parseQtyAny(fields[3], bl.quantity)) {
                warnings += "BillDetail.txt line " + std::to_string(lineNum) + ": Invalid quantity '" + fields[3] + "'. Skipped.\n";
                continue;
            }

            if (!parseMoney(fields[4], bl.discount).success) {
                warnings += "BillDetail.txt line " + std::to_string(lineNum) + ": Invalid discount '" + fields[4] + "'. Skipped.\n";
                continue;
            }

            // Attach to the corresponding bill
            bool attached = false;
            for (auto& b : bills_) {
                if (b.id == billId) {
                    b.lines.push_back(bl);
                    attached = true;
                    break;
                }
            }

            if (!attached) {
                warnings += "BillDetail.txt line " + std::to_string(lineNum) + ": Orphan detail for bill ID '" + billId + "'. Skipped.\n";
            }
        }
    }

    if (!warnings.empty()) {
        return {true, warnings};
    }

    return {true, "Bills loaded successfully."};
}

std::string BillStore::serializeBills() const {
    std::string out;
    for (const auto& b : bills_) {
        std::vector<std::string> fields = {
            b.id,
            quoteField(b.customerName),
            b.orderId,
            formatMoney(b.subtotal),
            formatMoney(b.tax),
            formatMoney(b.itemDiscount),
            formatMoney(b.payable),
            formatMoney(b.received),
            formatMoney(b.balance),
            orderStatusToText(b.status),
            formatMoney(b.seasonalDiscount),
            formatMoney(b.specialDiscount),
            b.date.empty() ? "2026-11-22" : b.date,
            b.time.empty() ? "14:05:09" : b.time
        };
        out += writeRecord(fields) + "\n";
    }
    return out;
}

std::string BillStore::serializeDetails() const {
    std::string out;
    for (const auto& b : bills_) {
        for (const auto& line : b.lines) {
            std::vector<std::string> fields = {
                b.id,
                line.acronym,
                formatMoney(line.unitPrice),
                formatQty(line.quantity),
                formatMoney(line.discount)
            };
            out += writeRecord(fields) + "\n";
        }
    }
    return out;
}

OperationResult BillStore::save(const std::string& billPath, const std::string& detailPath) const {
    if (!billPath.empty()) {
        std::ofstream out(billPath, std::ios::binary);
        if (!out.is_open()) {
            return {false, "Cannot open bill file for writing: " + billPath};
        }
        out << serializeBills();
    }

    if (!detailPath.empty()) {
        std::ofstream out(detailPath, std::ios::binary);
        if (!out.is_open()) {
            return {false, "Cannot open bill detail file for writing: " + detailPath};
        }
        out << serializeDetails();
    }

    return {true, "Bills saved successfully."};
}

OperationResult BillStore::save() const {
    return save(billPath_, detailPath_);
}

OperationResult BillStore::add(const Bill& bill) {
    if (findById(bill.id).has_value()) {
        return {false, "Bill ID already exists: " + bill.id};
    }
    bills_.push_back(bill);
    return {true, "Bill added."};
}

OperationResult BillStore::replace(const Bill& bill) {
    for (size_t i = 0; i < bills_.size(); ++i) {
        if (bills_[i].id == bill.id) {
            bills_[i] = bill;
            return {true, "Bill replaced."};
        }
    }
    return {false, "Bill not found: " + bill.id};
}

std::optional<Bill> BillStore::findById(const std::string& id) const {
    for (const auto& b : bills_) {
        if (b.id == id) return b;
    }
    return std::nullopt;
}

std::optional<Bill> BillStore::findByOrderId(const std::string& orderId) const {
    for (const auto& b : bills_) {
        if (b.orderId == orderId) return b;
    }
    return std::nullopt;
}

std::string BillStore::nextBillId() const {
    std::int64_t maxNum = 0;
    for (const auto& b : bills_) {
        try {
            std::int64_t val = std::stoll(b.id);
            if (val > maxNum) maxNum = val;
        } catch (...) {}
    }
    return padLeft(maxNum + 1, 9);
}
