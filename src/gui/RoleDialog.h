#pragma once
// OWNER: Member 1. Startup and switch-role dialog (Cashier or Manager with PIN).
#include <QDialog>
#include "Role.h"
#include "ManagerAuth.h"

class QRadioButton;
class QLineEdit;
class QLabel;
class QPushButton;

class RoleDialog : public QDialog {
    Q_OBJECT
public:
    explicit RoleDialog(const ManagerAuth& auth, QWidget* parent = nullptr);

    Role selectedRole() const { return selectedRole_; }

private slots:
    void onRoleToggled();
    void onContinueClicked();

private:
    void setupUi();

    const ManagerAuth& auth_;
    Role selectedRole_ = Role::Cashier;

    QRadioButton* cashierRadio_ = nullptr;
    QRadioButton* managerRadio_ = nullptr;
    QWidget* pinContainer_ = nullptr;
    QLineEdit* pinEdit_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QPushButton* continueBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
};
