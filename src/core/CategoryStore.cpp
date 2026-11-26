#include "CategoryStore.h"
#include "RecordFormat.h"
#include <sstream>

namespace {
bool validCategoryId(const std::string& id)
{
    if (id.size() != 2) return false;
    if (id[0] < '1' || id[0] > '9') return false;
    return id[1] >= '0' && id[1] <= '9';
}
}

OperationResult CategoryStore::validate(const Category& c, const std::string& ignoreId) const
{
    OperationResult r;
    if (!validCategoryId(c.id)) { r.message = "Category id must be 2 digits from 10 to 99."; return r; }
    if (c.name.empty()) { r.message = "Category name is required."; return r; }
    if (c.acronym.empty() || c.acronym.size() > 8) { r.message = "Acronym must be 1 to 8 characters."; return r; }
    for (auto& e : items_) {
        if (e.id == ignoreId) continue;
        if (e.id == c.id) { r.message = "Category id already exists."; return r; }
        if (e.name == c.name) { r.message = "Category name already exists."; return r; }
    }
    r.success = true;
    return r;
}

OperationResult CategoryStore::load(const std::string& path)
{
    items_.clear();
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
        if (f.size() != 3) {
            warnings_.push_back("line " + std::to_string(lineNo) + ": expected 3 fields (row skipped)");
            continue;
        }
        Category c{f[0], f[1], f[2]};
        OperationResult v = validate(c, "");
        if (!v.success) {
            warnings_.push_back("line " + std::to_string(lineNo) + ": " + v.message + " (row skipped)");
            continue;
        }
        items_.push_back(c);
    }
    OperationResult ok;
    ok.success = true;
    ok.message = std::to_string(items_.size()) + " categories loaded";
    if (!warnings_.empty()) ok.message += ", " + std::to_string(warnings_.size()) + " rows skipped";
    return ok;
}

std::string CategoryStore::serialize() const
{
    std::ostringstream out;
    for (auto& c : items_)
        out << writeRecord({c.id, quoteField(c.name), c.acronym}) << "\n";
    return out.str();
}

OperationResult CategoryStore::save(const std::string& path) const
{
    FileTransaction t;
    t.add(path, serialize());
    return t.commit();
}

OperationResult CategoryStore::add(const Category& c)
{
    OperationResult r = validate(c, "");
    if (!r.success) return r;
    items_.push_back(c);
    r.message = "Category added.";
    return r;
}

OperationResult CategoryStore::update(const Category& c)
{
    OperationResult r;
    for (auto& e : items_) {
        if (e.id != c.id) continue;
        r = validate(c, c.id);
        if (!r.success) return r;
        e = c;
        r.message = "Category updated.";
        return r;
    }
    r.message = "Category not found.";
    return r;
}

std::optional<Category> CategoryStore::findById(const std::string& id) const
{
    for (auto& c : items_) if (c.id == id) return c;
    return std::nullopt;
}