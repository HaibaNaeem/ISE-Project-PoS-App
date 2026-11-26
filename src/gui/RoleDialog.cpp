// OWNER: Member 1. Startup and switch-role dialog (Cashier or Manager with PIN).
#include "RoleDialog.h"
#include "DialogStyle.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

RoleDialog::RoleDialog(const ManagerAuth& auth, QWidget* parent)
    : QDialog(parent), auth_(auth) {
    setWindowTitle("Select Role - PoS V2");
    setMinimumWidth(400);
    setStyleSheet(DialogStyle::dialogStyleSheet());
    setupUi();
    onRoleToggled();
}

void RoleDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* titleLbl = new QLabel("Sign In to Point of Sale", this);
    titleLbl->setStyleSheet(QString("font-size: 15pt; font-weight: bold; color: %1;").arg(DialogStyle::kPrimary));
    root->addWidget(titleLbl);

    auto* roleBox = new QGroupBox("Choose Role", this);
    auto* roleLayout = new QVBoxLayout(roleBox);
    roleLayout->setSpacing(8);

    cashierRadio_ = new QRadioButton("Cashier (Record orders, hold/resume, take payment, print bill)", roleBox);
    managerRadio_ = new QRadioButton("Manager (Full access: products, categories, inventory, seasons, reports)", roleBox);
    cashierRadio_->setChecked(true);

    auto* group = new QButtonGroup(this);
    group->addButton(cashierRadio_);
    group->addButton(managerRadio_);

    roleLayout->addWidget(cashierRadio_);
    roleLayout->addWidget(managerRadio_);
    root->addWidget(roleBox);

    pinContainer_ = new QWidget(this);
    auto* pinForm = new QFormLayout(pinContainer_);
    pinForm->setContentsMargins(0, 0, 0, 0);
    pinEdit_ = new QLineEdit(pinContainer_);
    pinEdit_->setEchoMode(QLineEdit::Password);
    pinEdit_->setMaxLength(4);
    pinEdit_->setPlaceholderText("Enter 4-digit manager PIN");
    pinForm->addRow("Manager PIN:", pinEdit_);
    root->addWidget(pinContainer_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 10pt;").arg(DialogStyle::kDanger));
    errorLabel_->hide();
    root->addWidget(errorLabel_);

    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(10);

    cancelBtn_ = new QPushButton("Cancel", this);
    cancelBtn_->setObjectName("cancelBtn");
    continueBtn_ = new QPushButton("Continue", this);
    continueBtn_->setObjectName("confirmBtn");
    continueBtn_->setDefault(true);

    btnLayout->addWidget(cancelBtn_);
    btnLayout->addWidget(continueBtn_);
    root->addLayout(btnLayout);

    connect(cashierRadio_, &QRadioButton::toggled, this, &RoleDialog::onRoleToggled);
    connect(managerRadio_, &QRadioButton::toggled, this, &RoleDialog::onRoleToggled);
    connect(pinEdit_, &QLineEdit::textChanged, this, [this] { errorLabel_->hide(); });
    connect(continueBtn_, &QPushButton::clicked, this, &RoleDialog::onContinueClicked);
    connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);
}

void RoleDialog::onRoleToggled() {
    errorLabel_->hide();
    const bool isManager = managerRadio_->isChecked();
    pinContainer_->setVisible(isManager);
    if (isManager) {
        pinEdit_->clear();
        pinEdit_->setFocus();
    } else {
        continueBtn_->setFocus();
    }
    adjustSize();
}

void RoleDialog::onContinueClicked() {
    if (cashierRadio_->isChecked()) {
        selectedRole_ = Role::Cashier;
        accept();
        return;
    }

    const std::string enteredPin = pinEdit_->text().trimmed().toStdString();
    if (!auth_.check(enteredPin)) {
        errorLabel_->setText("Incorrect manager PIN. Please try again.");
        errorLabel_->show();
        pinEdit_->selectAll();
        pinEdit_->setFocus();
        return;
    }

    selectedRole_ = Role::Manager;
    accept();
}
