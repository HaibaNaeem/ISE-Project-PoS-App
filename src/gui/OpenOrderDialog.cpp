// OWNER: Member 3 (replaces HeldOrdersDialog).
#include "OpenOrderDialog.h"
#include "BillDialog.h"
#include "EditPolicy.h"
#include "OrderEditService.h"
#include <QDate>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <algorithm>

namespace {

int statusSortRank(OrderStatus status) {
    switch (status) {
        case OrderStatus::OnHold:     return 0; // Held orders on top
        case OrderStatus::InProgress: return 1;
        case OrderStatus::Paid:       return 2;
        case OrderStatus::Archived:   return 3;
    }
    return 4;
}

} // namespace

OpenOrderDialog::OpenOrderDialog(OrderManager& manager,
                                 const OrderStore& orders,
                                 const BillStore& bills,
                                 std::string today,
                                 std::function<bool()> requestManagerApproval,
                                 bool needsApproval,
                                 QWidget* parent)
    : QDialog(parent),
      manager_(manager),
      orders_(orders),
      bills_(bills),
      today_(std::move(today)),
      requestManagerApproval_(std::move(requestManagerApproval)),
      needsApproval_(needsApproval) {
    if (today_.empty()) {
        today_ = QDate::currentDate().toString("yyyy-MM-dd").toStdString();
    }

    setWindowTitle(tr("Open Order"));
    setMinimumSize(620, 440);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(tr("Find by order ID or customer name..."));

    list_ = new QListWidget(this);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->hide();

    openButton_ = new QPushButton(tr("Open / Resume"), this);
    openButton_->setEnabled(false);
    openButton_->setDefault(true);

    viewBillButton_ = new QPushButton(tr("View Bill"), this);
    viewBillButton_->setEnabled(false);

    closeButton_ = new QPushButton(tr("Close"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(viewBillButton_);
    buttons->addStretch(1);
    buttons->addWidget(openButton_);
    buttons->addWidget(closeButton_);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(searchEdit_);
    layout->addWidget(list_, 1);
    layout->addWidget(statusLabel_);
    layout->addLayout(buttons);

    connect(searchEdit_, &QLineEdit::textChanged, this, &OpenOrderDialog::onFilterChanged);
    connect(list_, &QListWidget::itemSelectionChanged, this, &OpenOrderDialog::onSelectionChanged);
    connect(list_, &QListWidget::itemDoubleClicked, this, [this] { onOpenClicked(); });
    connect(openButton_, &QPushButton::clicked, this, &OpenOrderDialog::onOpenClicked);
    connect(viewBillButton_, &QPushButton::clicked, this, &OpenOrderDialog::onViewBillClicked);
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::reject);

    populateList();
}

void OpenOrderDialog::setRequestManagerApproval(std::function<bool()> callback) {
    requestManagerApproval_ = std::move(callback);
}

void OpenOrderDialog::setNeedsApproval(bool needsApproval) {
    needsApproval_ = needsApproval;
    needsApprovalCheck_ = nullptr;
}

void OpenOrderDialog::setNeedsApprovalCheck(std::function<bool()> callback) {
    needsApprovalCheck_ = std::move(callback);
}

void OpenOrderDialog::populateList() {
    list_->clear();

    const QString filter = searchEdit_->text().trimmed();
    std::vector<Order> sorted = orders_.orders();
    std::stable_sort(sorted.begin(), sorted.end(), [](const Order& a, const Order& b) {
        return statusSortRank(a.status) < statusSortRank(b.status);
    });

    for (const Order& o : sorted) {
        const QString idStr = QString::fromStdString(o.id);
        const QString customer = o.customerName.empty()
                                     ? tr("Walk-in")
                                     : QString::fromStdString(o.customerName);

        if (!filter.isEmpty() &&
            !idStr.contains(filter, Qt::CaseInsensitive) &&
            !customer.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        QString statusText = QString::fromStdString(orderStatusToText(o.status));
        if (o.status == OrderStatus::Paid) {
            const std::string bDate = billDateForOrder(o);
            const std::string until = EditPolicy::editWindowUntilDate(bDate);
            if (!until.empty()) {
                if (EditPolicy::withinEditWindow(bDate, today_)) {
                    statusText = tr("paid — editable until %1").arg(QString::fromStdString(until));
                } else {
                    statusText = tr("paid — editable until %1 (expired)").arg(QString::fromStdString(until));
                }
            }
        }

        const QString text = tr("Order %1  |  %2  |  %3 %4  |  %5 item(s)  |  %6")
                                 .arg(idStr,
                                      customer,
                                      QString::fromStdString(o.date),
                                      QString::fromStdString(o.time))
                                 .arg(static_cast<qulonglong>(o.lines.size()))
                                 .arg(statusText);

        auto* item = new QListWidgetItem(text, list_);
        item->setData(Qt::UserRole, idStr);
    }

    onSelectionChanged();
}

std::optional<Order> OpenOrderDialog::selectedOrder() const {
    QListWidgetItem* item = list_->currentItem();
    if (!item) {
        return std::nullopt;
    }
    const std::string orderId = item->data(Qt::UserRole).toString().toStdString();
    return orders_.findById(orderId);
}

std::optional<Bill> OpenOrderDialog::findBillForOrder(const std::string& orderId) const {
    // TODO(M4): switch to bills_.findByOrderId(orderId) when M4 merges Increment 3 BillStore.
    std::optional<Bill> found;
    for (const auto& bill : bills_.bills()) {
        if (bill.orderId == orderId) {
            found = bill;
        }
    }
    return found;
}

std::string OpenOrderDialog::billDateForOrder(const Order& order) const {
    const auto billOpt = findBillForOrder(order.id);
    if (billOpt.has_value() && !billOpt->date.empty()) {
        return billOpt->date;
    }
    return order.date;
}

bool OpenOrderDialog::checkManagerApproval() const {
    const bool mustAsk = needsApprovalCheck_ ? needsApprovalCheck_() : needsApproval_;
    if (!mustAsk) {
        return true;
    }
    if (!requestManagerApproval_) {
        return false; // Safe default when M1 callback is not wired yet
    }
    return requestManagerApproval_();
}

void OpenOrderDialog::showStatus(const OperationResult& result) {
    statusLabel_->setText(QString::fromStdString(result.message));
    statusLabel_->setStyleSheet(result.success
                                    ? "background:#e6f4ea; color:#1e4620; padding:6px; border:1px solid #a3cfbb;"
                                    : "background:#fdecea; color:#7a1c1c; padding:6px; border:1px solid #f1aeb5;");
    statusLabel_->show();
}

void OpenOrderDialog::onFilterChanged(const QString&) {
    populateList();
}

void OpenOrderDialog::onSelectionChanged() {
    const auto ordOpt = selectedOrder();
    const bool hasSelection = ordOpt.has_value() && !list_->selectedItems().isEmpty();
    openButton_->setEnabled(hasSelection);

    const bool hasBill = hasSelection && findBillForOrder(ordOpt->id).has_value();
    viewBillButton_->setEnabled(hasBill);
}

void OpenOrderDialog::onOpenClicked() {
    const auto ordOpt = selectedOrder();
    if (!ordOpt.has_value()) {
        return;
    }

    bool approved = false;
    if (ordOpt->status == OrderStatus::Paid) {
        // Only ask for manager PIN if the paid order is still within the 7-day window;
        // if not, let OrderEditService::open return the core's window-expired message.
        if (EditPolicy::withinEditWindow(billDateForOrder(*ordOpt), today_)) {
            approved = checkManagerApproval();
        }
    }

    const OperationResult res = OrderEditService::open(
        manager_, orders_, bills_, ordOpt->id, today_, approved);
    if (!res.success) {
        showStatus(res);
        return;
    }

    selectedId_ = QString::fromStdString(ordOpt->id);
    accept();
}

void OpenOrderDialog::onViewBillClicked() {
    const auto ordOpt = selectedOrder();
    if (!ordOpt.has_value()) {
        return;
    }
    const auto billOpt = findBillForOrder(ordOpt->id);
    if (!billOpt.has_value()) {
        showStatus({false, "No bill found for order " + ordOpt->id + "."});
        return;
    }

    BillDialog dlg(*billOpt, this);
    dlg.exec();
}
