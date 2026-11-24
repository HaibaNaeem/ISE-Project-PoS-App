#pragma once
// OWNER: Member 2. Add / edit one product. Add: barcode and image are created on Save.
// Edit: barcode, category and unit are read-only.
#include <QDialog>
#include <string>
#include "CategoryStore.h"
#include "Product.h"
#include "ProductCatalog.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class ProductEditorDialog : public QDialog {
    Q_OBJECT
public:
    // editing == nullptr -> add mode
    ProductEditorDialog(ProductCatalog& catalog, const CategoryStore& categories,
                        const std::string& productPath, const Product* editing,
                        QWidget* parent = nullptr);

private slots:
    void onCategoryChanged();
    void onSave();

private:
    void showImage(const QString& relativePath);
    void showError(const QString& text);

    ProductCatalog& catalog_;
    std::string path_;
    bool editing_ = false;
    Product original_;

    QComboBox* category_ = nullptr;
    QComboBox* unit_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* acronym_ = nullptr;
    QLineEdit* price_ = nullptr;
    QLineEdit* tax_ = nullptr;
    QLineEdit* discount_ = nullptr;
    QLineEdit* stock_ = nullptr;
    QLabel* barcode_ = nullptr;
    QLabel* image_ = nullptr;
    QLabel* message_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
};