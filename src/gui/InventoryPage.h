#pragma once
// OWNER: Member 3. Manager inventory screen: view stock, search, set or receive stock.
// Contains no role/access checks (MainWindow decides visibility via AccessPolicy).
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>
#include "InventoryService.h"

class InventoryPage : public QWidget {
    Q_OBJECT
public:
    explicit InventoryPage(InventoryService& inventory, QWidget* parent = nullptr);

public slots:
    void refresh();

private slots:
    void onSearchChanged(const QString& text);
    void onSelectionChanged();
    void onSetStock();
    void onReceiveStock();

private:
    void applyAction(bool isReceive);
    QString selectedProductId() const;
    void showStatus(const OperationResult& result);

    InventoryService& inventory_;
    QLineEdit* searchEdit_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLineEdit* qtyEdit_ = nullptr;
    QPushButton* setButton_ = nullptr;
    QPushButton* receiveButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};
