#pragma once
// OWNER: Member 4.
// Cashier Payment Dialog for Increment 3:
// Displays payable in large text, pre-selects Cash, pre-fills amount received with exact payable,
// computes change live using core calculation, shows merchant field only for card/wallet,
// and supports manager-approved special discount up to 8.00%.
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <functional>
#include "Bill.h"
#include "Units.h"

class PaymentDialog : public QDialog {
    Q_OBJECT

public:
    explicit PaymentDialog(Money payableAmount, QWidget* parent = nullptr);
    PaymentDialog(Money payableAmount, bool isManager,
                  std::function<bool()> approvalCallback = nullptr,
                  QWidget* parent = nullptr);

    // Get the validated payment information chosen by cashier
    PaymentInfo paymentInfo() const;

    // Get the cash tendered amount (in paisa; 0 if non-cash)
    Money tenderedAmount() const;

    // Get the calculated change amount (in paisa; 0 if non-cash)
    Money changeAmount() const;

    // Get the special discount percent applied (0 to 800)
    Percent specialPercent() const { return specialPercent_; }
    bool isSpecialApproved() const { return specialApproved_; }

private slots:
    void onPaymentModeChanged(int id);
    void onTenderedTextChanged(const QString& text);
    void onSpecialDiscountClicked();
    void onConfirmClicked();

private:
    void setupUi();
    void updateModeFields();
    void updateChangeCalculation();
    void recalculateTotals();

    Money basePayableAmount_ = 0;   // Original payable before special discount
    Money currentPayable_ = 0;       // Current payable after special discount
    Money tenderedAmount_ = 0;
    Money changeAmount_ = 0;
    Percent specialPercent_ = 0;     // Hundredths of a percent (e.g. 500 = 5.00%)
    bool specialApproved_ = false;
    bool isManager_ = true;
    std::function<bool()> approvalCallback_;

    // UI elements
    QLabel* payableLabel_ = nullptr;
    QLabel* discountInfoLabel_ = nullptr;
    QButtonGroup* modeGroup_ = nullptr;
    QRadioButton* cashRadio_ = nullptr;
    QRadioButton* cardRadio_ = nullptr;
    QRadioButton* walletRadio_ = nullptr;

    QWidget* merchantContainer_ = nullptr;
    QLineEdit* merchantEdit_ = nullptr;

    QWidget* cashContainer_ = nullptr;
    QLineEdit* tenderedEdit_ = nullptr;
    QLabel* changeLabel_ = nullptr;

    QPushButton* specialDiscountBtn_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QPushButton* confirmBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
};
