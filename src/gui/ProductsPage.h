#pragma once
// OWNER: Member 2. Manager screen: product table, add, edit, details, barcode search.
#include <QWidget>
#include <string>
#include "CategoryStore.h"
#include "ProductCatalog.h"

class QLabel;
class QLineEdit;
class QTableWidget;

class ProductsPage : public QWidget {
    Q_OBJECT
public:
    ProductsPage(ProductCatalog& catalog, CategoryStore& categories,
                 const std::string& productPath, QWidget* parent = nullptr);
    void refresh();

private slots:
    void onAdd();
    void onEdit();
    void onDetails();
    void onSearch(const QString& text);

private:
    const Product* selectedProduct() const;

    ProductCatalog& catalog_;
    CategoryStore& categories_;
    std::string path_;
    QTableWidget* table_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLabel* status_ = nullptr;
};