#pragma once
// OWNER: Member 1. Order screen layout and cashier interactions.
#include <QWidget>
#include <QString>
#include <optional>
#include <string>

#include "ProductCatalog.h"
#include "OrderManager.h"
#include "CheckoutService.h"
#include "CommonTypes.h"
#include "Role.h"
#include "ManagerAuth.h"

class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QTableWidget;
class QPushButton;
class CameraPanel;

class OrderPage : public QWidget {
    Q_OBJECT
public:
    OrderPage(ProductCatalog& catalog,
              OrderManager& orders,
              CheckoutService& checkout,
              Role role,
              const ManagerAuth& auth,
              QWidget* parent = nullptr);
    ~OrderPage() override;

    void setRole(Role role);
    void startCamera();
    void stopCamera();
    void showResult(const OperationResult& r);

signals:
    void statusMessage(const QString& message, bool success);

public slots:
    void addBarcode(const QString& code);

private slots:
    void onManualAdd();
    void onDescription();
    void onPlus();
    void onMinus();
    void onRemove();
    void onHold();
    void onResumeHeld(const std::string& orderId);
    void onOpenOrder();
    void onPay();
    void onCancel();
    void onSaveEdited();
    void onDiscardEdit();

private:
    void buildUi();
    void refresh();
    void refreshHeldStrip();
    void updateEditBanner(const std::optional<Order>& originalOrder);
    QString selectedProductId() const;
    void changeSelectedQuantity(int direction);

    ProductCatalog& catalog_;
    OrderManager& orders_;
    CheckoutService& checkout_;
    Role role_;
    const ManagerAuth& auth_;
    QString lastBarcode_;
    bool editingPaidOrder_ = false;

    QFrame* editBanner_ = nullptr;
    QLabel* editBannerLabel_ = nullptr;
    QPushButton* saveEditBtn_ = nullptr;
    QPushButton* discardEditBtn_ = nullptr;

    QWidget* heldStripHost_ = nullptr;
    QHBoxLayout* heldStripLayout_ = nullptr;
    QLabel* noHeldLabel_ = nullptr;

    CameraPanel* cameraPanel_ = nullptr;
    QLineEdit* barcodeEdit_ = nullptr;
    QDoubleSpinBox* qtyBox_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* contactEdit_ = nullptr;
    QTableWidget* cart_ = nullptr;
    QLabel *subtotalLbl_ = nullptr, *taxLbl_ = nullptr,
           *discountLbl_ = nullptr, *seasonalLbl_ = nullptr,
           *payableLbl_ = nullptr, *banner_ = nullptr;
    QPushButton *descBtn_ = nullptr, *plusBtn_ = nullptr, *minusBtn_ = nullptr,
                *removeBtn_ = nullptr, *holdBtn_ = nullptr, *openOrderBtn_ = nullptr,
                *payBtn_ = nullptr, *cancelBtn_ = nullptr;
};
