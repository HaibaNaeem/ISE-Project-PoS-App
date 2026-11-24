// OWNER: Member 1. Asks for the 4-digit manager PIN to approve a single restricted action (used by M3 and M4).
#include "ApprovalDialog.h"
#include "DialogStyle.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ApprovalDialog::ApprovalDialog(const ManagerAuth& auth,
                               QWidget* parent,
                               const QString& actionDescription)
    : QDialog(parent), auth_(auth) {
    setWindowTitle("Manager Approval Required");
    setMinimumWidth(380);
    setStyleSheet(DialogStyle::dialogStyleSheet());
    setupUi(actionDescription);
}

void ApprovalDialog::setupUi(const QString& actionDescription) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    const QString desc = actionDescription.trimmed().isEmpty()
        ? QString("This action requires manager approval. Enter the 4-digit manager PIN:")
        : actionDescription;
    auto* descLbl = new QLabel(desc, this);
    descLbl->setWordWrap(true);
    descLbl->setStyleSheet("font-weight: bold;");
    root->addWidget(descLbl);

    auto* form = new QFormLayout;
    pinEdit_ = new QLineEdit(this);
    pinEdit_->setEchoMode(QLineEdit::Password);
    pinEdit_->setMaxLength(4);
    pinEdit_->setPlaceholderText("4-digit manager PIN");
    form->addRow("Manager PIN:", pinEdit_);
    root->addLayout(form);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 10pt;").arg(DialogStyle::kDanger));
    errorLabel_->hide();
    root->addWidget(errorLabel_);

    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(10);

    cancelBtn_ = new QPushButton("Cancel", this);
    cancelBtn_->setObjectName("cancelBtn");
    approveBtn_ = new QPushButton("Approve", this);
    approveBtn_->setObjectName("confirmBtn");
    approveBtn_->setDefault(true);

    btnLayout->addWidget(cancelBtn_);
    btnLayout->addWidget(approveBtn_);
    root->addLayout(btnLayout);

    connect(pinEdit_, &QLineEdit::textChanged, this, [this] { errorLabel_->hide(); });
    connect(approveBtn_, &QPushButton::clicked, this, &ApprovalDialog::onApproveClicked);
    connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);

    pinEdit_->setFocus();
}

void ApprovalDialog::onApproveClicked() {
    const std::string enteredPin = pinEdit_->text().trimmed().toStdString();
    if (!auth_.check(enteredPin)) {
        errorLabel_->setText("Incorrect manager PIN. Approval denied.");
        errorLabel_->show();
        pinEdit_->selectAll();
        pinEdit_->setFocus();
        return;
    }
    accept();
}

bool ApprovalDialog::ask(const ManagerAuth& auth,
                         QWidget* parent,
                         const QString& actionDescription) {
    ApprovalDialog dlg(auth, parent, actionDescription);
    return dlg.exec() == QDialog::Accepted;
}
