// OWNER: Member 4.
// Cashier Payment Dialog implementation for Increment 3.
#include "PaymentDialog.h"
#include "DialogStyle.h"
#include "Pricing.h"
#include "DiscountRules.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>

namespace {

QString guiFormatMoney(Money amount) {
    return QString::fromStdString(formatMoney(amount) + " PKR");
}

bool guiParseMoney(const QString& text, Money& out) {
    std::string str = text.trimmed().toStdString();
    OperationResult r = parseMoney(str, out);
    return r.success;
}

} // anonymous namespace

PaymentDialog::PaymentDialog(Money payableAmount, QWidget* parent)
    : PaymentDialog(payableAmount, true, nullptr, parent) {}

PaymentDialog::PaymentDialog(Money payableAmount, bool isManager,
                             std::function<bool()> approvalCallback,
                             QWidget* parent)
    : QDialog(parent), basePayableAmount_(payableAmount), currentPayable_(payableAmount),
      isManager_(isManager), approvalCallback_(std::move(approvalCallback)) {
    setWindowTitle("Checkout - Payment");
    setMinimumWidth(460);
    setStyleSheet(DialogStyle::dialogStyleSheet());

    setupUi();
    updateModeFields();
    recalculateTotals();
}

void PaymentDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(14);

    // 1. Total Payable banner (Large bold text)
    auto* payableBox = new QGroupBox("Total Amount Due", this);
    auto* payableLayout = new QVBoxLayout(payableBox);
    payableLabel_ = new QLabel(guiFormatMoney(currentPayable_), payableBox);
    payableLabel_->setAlignment(Qt::AlignCenter);
    payableLabel_->setStyleSheet(QString("font-size: 22pt; font-weight: bold; color: %1; padding: 6px;")
                                .arg(DialogStyle::kPrimary));
    payableLayout->addWidget(payableLabel_);

    discountInfoLabel_ = new QLabel("", payableBox);
    discountInfoLabel_->setAlignment(Qt::AlignCenter);
    discountInfoLabel_->setStyleSheet("font-size: 10pt; color: #16A34A; font-weight: bold;");
    discountInfoLabel_->setVisible(false);
    payableLayout->addWidget(discountInfoLabel_);

    mainLayout->addWidget(payableBox);

    // 2. Special Discount button
    auto* discountLayout = new QHBoxLayout();
    specialDiscountBtn_ = new QPushButton("Apply Special Discount...", this);
    specialDiscountBtn_->setStyleSheet("QPushButton { background-color: #F8FAFC; border: 1px solid #CBD5E1; color: #334155; padding: 6px 12px; font-weight: bold; border-radius: 4px; }"
                                       "QPushButton:hover { background-color: #E2E8F0; }");
    connect(specialDiscountBtn_, &QPushButton::clicked, this, &PaymentDialog::onSpecialDiscountClicked);
    discountLayout->addWidget(specialDiscountBtn_);
    discountLayout->addStretch();
    mainLayout->addLayout(discountLayout);

    // 3. Payment Mode selection (Radio buttons)
    auto* modeBox = new QGroupBox("Select Payment Mode", this);
    auto* modeLayout = new QHBoxLayout(modeBox);
    modeGroup_ = new QButtonGroup(this);

    cashRadio_ = new QRadioButton("Cash", modeBox);
    cardRadio_ = new QRadioButton("Card", modeBox);
    walletRadio_ = new QRadioButton("Digital Wallet", modeBox);

    cashRadio_->setChecked(true); // Cash pre-selected as per spec
    modeGroup_->addButton(cashRadio_, static_cast<int>(PaymentMode::Cash));
    modeGroup_->addButton(cardRadio_, static_cast<int>(PaymentMode::Card));
    modeGroup_->addButton(walletRadio_, static_cast<int>(PaymentMode::Wallet));

    modeLayout->addWidget(cashRadio_);
    modeLayout->addWidget(cardRadio_);
    modeLayout->addWidget(walletRadio_);
    mainLayout->addWidget(modeBox);

    connect(modeGroup_, &QButtonGroup::idClicked, this, &PaymentDialog::onPaymentModeChanged);

    // 4. Cash details container (Tendered amount + Change Due)
    cashContainer_ = new QWidget(this);
    auto* cashLayout = new QFormLayout(cashContainer_);
    cashLayout->setContentsMargins(0, 0, 0, 0);

    tenderedEdit_ = new QLineEdit(cashContainer_);
    tenderedEdit_->setPlaceholderText("0.00");
    // Pre-fill amount received with exact payable amount as required by spec
    tenderedEdit_->setText(QString::fromStdString(formatMoney(currentPayable_)));

    changeLabel_ = new QLabel(guiFormatMoney(0), cashContainer_);
    changeLabel_->setStyleSheet(QString("font-weight: bold; font-size: 13pt; color: %1;")
                                .arg(DialogStyle::kSuccess));

    cashLayout->addRow("Amount Received (PKR):", tenderedEdit_);
    cashLayout->addRow("Change Returned:", changeLabel_);
    mainLayout->addWidget(cashContainer_);

    connect(tenderedEdit_, &QLineEdit::textChanged, this, &PaymentDialog::onTenderedTextChanged);

    // 5. Merchant container (Card & Wallet only)
    merchantContainer_ = new QWidget(this);
    auto* merchantLayout = new QFormLayout(merchantContainer_);
    merchantLayout->setContentsMargins(0, 0, 0, 0);

    merchantEdit_ = new QLineEdit(merchantContainer_);
    merchantEdit_->setPlaceholderText("e.g. HBL, Meezan, Easypaisa, JazzCash");
    merchantLayout->addRow("Merchant / Network:", merchantEdit_);
    mainLayout->addWidget(merchantContainer_);

    // 6. Error Label
    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet("color: #DC2626; font-weight: bold;");
    errorLabel_->setVisible(false);
    mainLayout->addWidget(errorLabel_);

    // 7. Buttons (Confirm / Cancel)
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    cancelBtn_ = new QPushButton("Cancel", this);
    confirmBtn_ = new QPushButton("Confirm Payment", this);
    confirmBtn_->setDefault(true); // Enter key confirms payment as per spec

    btnLayout->addWidget(cancelBtn_);
    btnLayout->addWidget(confirmBtn_);
    mainLayout->addLayout(btnLayout);

    connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmBtn_, &QPushButton::clicked, this, &PaymentDialog::onConfirmClicked);
    connect(tenderedEdit_, &QLineEdit::returnPressed, this, &PaymentDialog::onConfirmClicked);
    connect(merchantEdit_, &QLineEdit::returnPressed, this, &PaymentDialog::onConfirmClicked);
}

void PaymentDialog::updateModeFields() {
    PaymentMode mode = static_cast<PaymentMode>(modeGroup_->checkedId());
    bool isCash = (mode == PaymentMode::Cash);

    cashContainer_->setVisible(isCash);
    merchantContainer_->setVisible(!isCash);

    if (isCash) {
        tenderedEdit_->setFocus();
        tenderedEdit_->selectAll();
        updateChangeCalculation();
    } else {
        merchantEdit_->setFocus();
    }
}

void PaymentDialog::onPaymentModeChanged(int) {
    errorLabel_->setVisible(false);
    updateModeFields();
}

void PaymentDialog::onTenderedTextChanged(const QString&) {
    errorLabel_->setVisible(false);
    updateChangeCalculation();
}

void PaymentDialog::updateChangeCalculation() {
    Money tendered = 0;
    if (guiParseMoney(tenderedEdit_->text(), tendered)) {
        tenderedAmount_ = tendered;
        // Pure financial calculation via core changeDue()
        changeAmount_ = changeDue(currentPayable_, tenderedAmount_);
        changeLabel_->setText(guiFormatMoney(changeAmount_));
        if (tenderedAmount_ < currentPayable_) {
            changeLabel_->setStyleSheet("font-weight: bold; font-size: 13pt; color: #DC2626;");
        } else {
            changeLabel_->setStyleSheet(QString("font-weight: bold; font-size: 13pt; color: %1;")
                                        .arg(DialogStyle::kSuccess));
        }
    } else {
        tenderedAmount_ = 0;
        changeAmount_ = 0;
        changeLabel_->setText(guiFormatMoney(0));
    }
}

void PaymentDialog::onSpecialDiscountClicked() {
    // If cashier: requires manager approval
    if (!isManager_) {
        bool approved = false;
        if (approvalCallback_) {
            approved = approvalCallback_();
        } else {
            // Placeholder PIN check until M1's ApprovalDialog is merged
            bool ok = false;
            QString pin = QInputDialog::getText(this, "Manager Approval Required",
                                                "Cashier requires Manager PIN to apply special discount:\nEnter Manager PIN:",
                                                QLineEdit::Password, "", &ok);
            if (ok && pin == "1234") {
                approved = true;
            } else if (ok) {
                QMessageBox::warning(this, "Approval Failed", "Incorrect Manager PIN.");
                return;
            }
        }

        if (!approved) {
            return;
        }
        specialApproved_ = true;
    } else {
        specialApproved_ = true;
    }

    bool ok = false;
    double percentInput = QInputDialog::getDouble(
        this, "Apply Special Discount",
        "Enter Special Discount Percentage (Maximum 8.00%):",
        specialPercent_ / 100.0, 0.0, 8.0, 2, &ok
    );

    if (ok) {
        Percent p = static_cast<Percent>(percentInput * 100.0 + 0.5);
        OperationResult valRes = SpecialDiscountRule::validatePercent(p);
        if (!valRes.success) {
            QMessageBox::warning(this, "Invalid Discount", QString::fromStdString(valRes.message));
            return;
        }

        specialPercent_ = p;
        recalculateTotals();
    }
}

void PaymentDialog::recalculateTotals() {
    if (specialPercent_ > 0) {
        Money discAmt = BillingMath::percentOf(basePayableAmount_, specialPercent_);
        currentPayable_ = (basePayableAmount_ > discAmt) ? (basePayableAmount_ - discAmt) : 0;
        discountInfoLabel_->setText(QString("Special Discount: %1% (-%2)")
                                    .arg(QString::number(specialPercent_ / 100.0, 'f', 2))
                                    .arg(guiFormatMoney(discAmt)));
        discountInfoLabel_->setVisible(true);
    } else {
        currentPayable_ = basePayableAmount_;
        discountInfoLabel_->setVisible(false);
    }

    payableLabel_->setText(guiFormatMoney(currentPayable_));
    if (cashRadio_->isChecked()) {
        tenderedEdit_->setText(QString::fromStdString(formatMoney(currentPayable_)));
        updateChangeCalculation();
    }
}

void PaymentDialog::onConfirmClicked() {
    errorLabel_->setVisible(false);
    PaymentMode mode = static_cast<PaymentMode>(modeGroup_->checkedId());

    if (mode == PaymentMode::Cash) {
        if (!guiParseMoney(tenderedEdit_->text(), tenderedAmount_)) {
            errorLabel_->setText("Please enter a valid cash amount.");
            errorLabel_->setVisible(true);
            return;
        }

        if (tenderedAmount_ < currentPayable_) {
            errorLabel_->setText("Amount received is less than the payable amount (" +
                                 guiFormatMoney(currentPayable_) + ").");
            errorLabel_->setVisible(true);
            return;
        }
    } else {
        // Card or Wallet
        QString merchant = merchantEdit_->text().trimmed();
        if (merchant.isEmpty()) {
            errorLabel_->setText("Please enter the merchant / network name.");
            errorLabel_->setVisible(true);
            merchantEdit_->setFocus();
            return;
        }
        tenderedAmount_ = currentPayable_;
        changeAmount_ = 0;
    }

    accept();
}

PaymentInfo PaymentDialog::paymentInfo() const {
    PaymentInfo info;
    info.mode = static_cast<PaymentMode>(modeGroup_->checkedId());
    info.merchant = (info.mode == PaymentMode::Cash) ? "" : merchantEdit_->text().trimmed().toStdString();
    info.received = tenderedAmount_;
    info.specialPercent = specialPercent_;
    info.specialApproved = specialApproved_;
    return info;
}

Money PaymentDialog::tenderedAmount() const {
    return tenderedAmount_;
}

Money PaymentDialog::changeAmount() const {
    return changeAmount_;
}
