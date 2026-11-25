#pragma once
// OWNER: Member 3 (replaces HeldOrdersDialog).
// Lists held orders on top, supports searching by order ID or customer name,
// shows "editable until <date>" for paid orders, allows viewing bills via BillDialog,
// and delegates opening/validation to OrderEditService.
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Bill.h"
#include "BillStore.h"
#include "Order.h"
#include "OrderManager.h"
#include "OrderStore.h"

class OpenOrderDialog : public QDialog {
    Q_OBJECT
public:
    // MEMBER 1 HOOKUP:
    // - requestManagerApproval: lambda showing M1's ApprovalDialog and returning true if PIN accepted.
    // - needsApproval: M1 passes AccessPolicy::needsManagerApproval(role, Action::EditPaidOrder)
    //   (either as a bool or via setNeedsApproval / setNeedsApprovalCheck).
    // Safe defaults: needsApproval = true and requestManagerApproval returns false (cashier cannot open paid order).
    OpenOrderDialog(OrderManager& manager,
                    const OrderStore& orders,
                    const BillStore& bills,
                    std::string today = "",
                    std::function<bool()> requestManagerApproval = {},
                    bool needsApproval = true,
                    QWidget* parent = nullptr);

    void setRequestManagerApproval(std::function<bool()> callback);
    void setNeedsApproval(bool needsApproval);
    void setNeedsApprovalCheck(std::function<bool()> callback);

    QString selectedOrderId() const { return selectedId_; }

private slots:
    void onFilterChanged(const QString& text);
    void onSelectionChanged();
    void onOpenClicked();
    void onViewBillClicked();

private:
    void populateList();
    std::optional<Order> selectedOrder() const;
    std::optional<Bill> findBillForOrder(const std::string& orderId) const;
    std::string billDateForOrder(const Order& order) const;
    bool checkManagerApproval() const;
    void showStatus(const OperationResult& result);

    OrderManager& manager_;
    const OrderStore& orders_;
    const BillStore& bills_;
    std::string today_;
    std::function<bool()> requestManagerApproval_;
    std::function<bool()> needsApprovalCheck_;
    bool needsApproval_ = true;

    QLineEdit* searchEdit_ = nullptr;
    QListWidget* list_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* openButton_ = nullptr;
    QPushButton* viewBillButton_ = nullptr;
    QPushButton* closeButton_ = nullptr;
    QString selectedId_;
};
