#pragma once
// OWNER: Member 2. Manager screen: product table, add, edit, details, barcode search and camera scan.
#include <QWidget>
#include <string>
#include "CategoryStore.h"
#include "ProductCatalog.h"

class CameraPanel;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class ProductsPage : public QWidget {
    Q_OBJECT
public:
    ProductsPage(ProductCatalog& catalog, CategoryStore& categories,
                 const std::string& productPath, QWidget* parent = nullptr);
    void refresh();

protected:
    void hideEvent(QHideEvent* event) override;   // leaving this screen stops the camera

private slots:
    void onAdd();
    void onEdit();
    void onDetails();
    void onSearch(const QString& text);
    void onSearchEnter();
    void onToggleCamera();
    void onBarcodeScanned(const QString& code);

private:
    const Product* selectedProduct() const;
    bool showInfoForBarcode(const QString& code);
    void stopCamera();

    ProductCatalog& catalog_;
    CategoryStore& categories_;
    std::string path_;
    QTableWidget* table_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* cameraBtn_ = nullptr;
    CameraPanel* camera_ = nullptr;
    bool busy_ = false;        // true while a product info dialog is open
};