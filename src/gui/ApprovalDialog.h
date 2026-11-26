#pragma once
// OWNER: Member 1. Asks for the 4-digit manager PIN to approve a single restricted action (used by M3 and M4).
#include <QDialog>
#include <QString>
#include "ManagerAuth.h"

class QLineEdit;
class QLabel;
class QPushButton;

class ApprovalDialog : public QDialog {
    Q_OBJECT
public:
    explicit ApprovalDialog(const ManagerAuth& auth,
                            QWidget* parent = nullptr,
                            const QString& actionDescription = QString());

    // Convenience helper: returns true if the manager PIN was entered and verified, false if cancelled.
    static bool ask(const ManagerAuth& auth,
                    QWidget* parent = nullptr,
                    const QString& actionDescription = QString());

private slots:
    void onApproveClicked();

private:
    void setupUi(const QString& actionDescription);

    const ManagerAuth& auth_;
    QLineEdit* pinEdit_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QPushButton* approveBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
};
