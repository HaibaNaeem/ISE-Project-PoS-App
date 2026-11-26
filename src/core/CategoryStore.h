#pragma once
#include <optional>
#include <string>
#include <vector>
#include "CommonTypes.h"
#include "Category.h"

class CategoryStore {
public:
    OperationResult load(const std::string& path);
    OperationResult save(const std::string& path) const;
    std::string serialize() const;

    OperationResult add(const Category& c);      // id 10-99 unique, name unique, acronym <= 8
    OperationResult update(const Category& c);   // rename: id stays, name/acronym change
    std::optional<Category> findById(const std::string& id) const;
    std::vector<Category> all() const { return items_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    OperationResult validate(const Category& c, const std::string& ignoreId) const;
    std::vector<Category> items_;
    std::vector<std::string> warnings_;
};