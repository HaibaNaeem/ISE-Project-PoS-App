#include "ProductCatalog.h"
#include "CodeValidator.h"
#include "RecordFormat.h"
#include <sstream>

namespace {
bool parseProductRow(const std::vector<std::string>& f, Product& p, std::string& err)
{
    if (f.size() != 11) {
        err = "expected 11 fields (old format?), found " + std::to_string(f.size());
        return false;
    }
    p.id = f[0]; p.barcode = f[1]; p.categoryId = f[2]; p.acronym = f[3]; p.name = f[4];
    if (p.id.empty()) { err = "empty id"; return false; }
    if (!isValidSixDigitCode(p.barcode)) { err = "invalid barcode '" + p.barcode + "'"; return false; }
    if (p.categoryId != p.barcode.substr(0, 2)) { err = "category id must equal first two digits of barcode"; return false; }
    if (p.acronym.empty() || p.acronym.size() > 8) { err = "acronym must be 1 to 8 characters"; return false; }
    if (p.name.empty()) { err = "empty name"; return false; }

    OperationResult r = parseMeasureUnit(f[5], p.unit);
    if (!r.success) { err = r.message; return false; }
    r = parseMoney(f[6], p.unitPrice);        if (!r.success) { err = r.message; return false; }
    r = parseMoney(f[7], p.taxPerUnit);       if (!r.success) { err = r.message; return false; }
    r = parseMoney(f[8], p.discountPerUnit);  if (!r.success) { err = r.message; return false; }
    r = parseQuantity(f[9], p.unit, p.available);
    if (!r.success) { err = r.message; return false; }
    p.barcodeImagePath = f[10];
    return true;
}
}

ProductCatalog::ProductCatalog(std::vector<Product> products) : products_(std::move(products)) {}

bool ProductCatalog::acronymTaken(const std::string& acronym, const std::string& exceptId) const
{
    for (auto& e : products_)
        if (e.acronym == acronym && e.id != exceptId) return true;
    return false;
}

OperationResult ProductCatalog::load(const std::string& path)
{
    products_.clear();
    warnings_.clear();

    std::vector<std::string> lines;
    OperationResult r = readLines(path, lines);
    if (!r.success) return r;

    int lineNo = 0;
    for (auto& line : lines) {
        ++lineNo;
        std::vector<std::string> f;
        OperationResult s = splitRecord(line, lineNo, f);
        if (!s.success) { warnings_.push_back(s.message + " (row skipped)"); continue; }

        Product p;
        std::string err;
        if (!parseProductRow(f, p, err)) {
            warnings_.push_back("line " + std::to_string(lineNo) + ": " + err + " (row skipped)");
            continue;
        }
        bool dup = false;
        for (auto& e : products_)
            if (e.barcode == p.barcode || e.id == p.id || e.acronym == p.acronym) { dup = true; break; }
        if (dup) {
            warnings_.push_back("line " + std::to_string(lineNo) + ": duplicate id, barcode or acronym (row skipped)");
            continue;
        }
        products_.push_back(p);
    }

    OperationResult ok;
    ok.success = true;
    ok.message = std::to_string(products_.size()) + " products loaded";
    if (!warnings_.empty()) {
        ok.message += ", " + std::to_string(warnings_.size()) + " rows skipped";
        for (auto& w : warnings_) ok.message += "\n" + w;
    }
    return ok;
}

std::string ProductCatalog::serialize() const
{
    std::ostringstream out;
    for (auto& p : products_) {
        out << writeRecord({p.id, p.barcode, p.categoryId, p.acronym, quoteField(p.name),
                            measureUnitToText(p.unit),
                            formatMoney(p.unitPrice), formatMoney(p.taxPerUnit), formatMoney(p.discountPerUnit),
                            formatQuantity(p.available, p.unit), quoteField(p.barcodeImagePath)})
            << "\n";
    }
    return out.str();
}

OperationResult ProductCatalog::save(const std::string& path) const
{
    FileTransaction t;
    t.add(path, serialize());
    return t.commit();
}

std::optional<Product> ProductCatalog::findByBarcode(const std::string& barcode) const
{
    for (auto& p : products_) if (p.barcode == barcode) return p;
    return std::nullopt;
}

std::optional<Product> ProductCatalog::findById(const std::string& id) const
{
    for (auto& p : products_) if (p.id == id) return p;
    return std::nullopt;
}

OperationResult ProductCatalog::reduceStock(const std::string& productId, Quantity amount)
{
    OperationResult r;
    if (amount <= 0) { r.message = "Amount must be greater than zero."; return r; }
    for (auto& p : products_) {
        if (p.id != productId) continue;
        if (p.available < amount) { r.message = "Not enough stock for " + p.name + "."; return r; }
        p.available -= amount;
        r.success = true;
        r.message = "Stock updated.";
        return r;
    }
    r.message = "Product not found.";
    return r;
}

OperationResult ProductCatalog::checkFields(const Product& p) const
{
    OperationResult r;
    if (!isValidSixDigitCode(p.barcode)) { r.message = "Invalid barcode."; return r; }
    if (p.categoryId != p.barcode.substr(0, 2)) { r.message = "Category does not match the barcode."; return r; }
    if (p.acronym.empty() || p.acronym.size() > 8) { r.message = "Acronym must be 1 to 8 characters."; return r; }
    if (p.name.empty()) { r.message = "Product name is required."; return r; }
    if (p.unitPrice < 0 || p.taxPerUnit < 0 || p.discountPerUnit < 0) { r.message = "Price, tax and discount cannot be negative."; return r; }
    if (p.available < 0) { r.message = "Stock cannot be negative."; return r; }
    r.success = true;
    return r;
}

OperationResult ProductCatalog::add(const Product& in)
{
    Product p = in;
    if (p.id.empty()) p.id = nextProductId();
    OperationResult r = checkFields(p);
    if (!r.success) return r;
    for (auto& e : products_) {
        if (e.id == p.id) { r.success = false; r.message = "Product id already exists."; return r; }
        if (e.barcode == p.barcode) { r.success = false; r.message = "Barcode already exists."; return r; }
    }
    if (acronymTaken(p.acronym, "")) { r.success = false; r.message = "Acronym already used by another product."; return r; }
    products_.push_back(p);
    r.message = "Product added.";
    return r;
}

OperationResult ProductCatalog::update(const Product& in)
{
    OperationResult r;
    for (auto& e : products_) {
        if (e.id != in.id) continue;
        if (in.barcode != e.barcode || in.categoryId != e.categoryId) {
            r.message = "Barcode and category cannot be changed.";
            return r;
        }
        r = checkFields(in);
        if (!r.success) return r;
        if (acronymTaken(in.acronym, in.id)) { r.success = false; r.message = "Acronym already used by another product."; return r; }
        e = in;
        r.message = "Product updated.";
        return r;
    }
    r.message = "Product not found.";
    return r;
}

OperationResult ProductCatalog::setAvailable(const std::string& productId, Quantity q)
{
    OperationResult r;
    if (q < 0) { r.message = "Stock cannot be negative."; return r; }
    for (auto& p : products_)
        if (p.id == productId) { p.available = q; r.success = true; r.message = "Stock updated."; return r; }
    r.message = "Product not found.";
    return r;
}

std::string ProductCatalog::nextProductId() const
{
    long long maxId = 100;
    for (auto& p : products_) {
        try { maxId = std::max(maxId, std::stoll(p.id)); } catch (...) {}
    }
    return std::to_string(maxId + 1);
}