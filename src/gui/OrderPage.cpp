// OWNER: Member 1. Order screen layout and cashier interactions.
#include "OrderPage.h"
#include "AccessPolicy.h"
#include "ApprovalDialog.h"
#include "CameraPanel.h"
#include "DateText.h"
#include "OpenOrderDialog.h"
#include "OrderEditService.h"
#include "ProductInfoDialog.h"
#include "PaymentDialog.h"
#include "BillDialog.h"
#include "Pricing.h"
#include "Units.h"

#include <QtWidgets>
#include <cmath>

namespace {

QString moneyText(Money m) {
    return QString::fromStdString(formatMoney(m)) + " PKR";
}

QString qtyText(Quantity q, MeasureUnit u) {
    return QString::fromStdString(formatQuantity(q, u));
}

} // namespace

OrderPage::OrderPage(ProductCatalog& catalog,
                     OrderManager& orders,
                     const OrderStore& orderStore,
                     const BillStore& billStore,
                     CheckoutService& checkout,
                     const SeasonService& seasons,
                     Role role,
                     const ManagerAuth& auth,
                     QWidget* parent)
    : QWidget(parent),
      catalog_(catalog),
      orders_(orders),
      orderStore_(orderStore),
      billStore_(billStore),
      checkout_(checkout),
      seasons_(seasons),
      role_(role),
      auth_(auth) {
    buildUi();
    refresh();
}

OrderPage::~OrderPage() = default;

void OrderPage::setRole(Role role) {
    role_ = role;
    refresh();
}

void OrderPage::refreshView() {
    refresh();
}

void OrderPage::startCamera() {
    if (cameraPanel_) {
        QTimer::singleShot(80, cameraPanel_, &CameraPanel::start);
    }
}

void OrderPage::stopCamera() {
    if (cameraPanel_) {
        cameraPanel_->stop();
    }
}

void OrderPage::buildUi() {
    // Edit-paid-order banner (shown only when OrderManager::original() is set; waiting on M3/M4)
    editBanner_ = new QFrame(this);
    editBanner_->setStyleSheet(
        "QFrame { background-color: #FEF3C7; border: 1px solid #F59E0B; border-radius: 4px; padding: 6px; }");
    editBannerLabel_ = new QLabel(editBanner_);
    editBannerLabel_->setStyleSheet("color: #92400E; font-weight: bold; font-size: 11pt;");
    saveEditBtn_ = new QPushButton("Save Changes", editBanner_);
    saveEditBtn_->setStyleSheet("background-color: #15803D; color: white; font-weight: bold; padding: 4px 12px;");
    discardEditBtn_ = new QPushButton("Discard", editBanner_);
    discardEditBtn_->setStyleSheet("background-color: #B91C1C; color: white; font-weight: bold; padding: 4px 12px;");

    auto* editLayout = new QHBoxLayout(editBanner_);
    editLayout->setContentsMargins(8, 4, 8, 4);
    editLayout->addWidget(editBannerLabel_, 1);
    editLayout->addWidget(saveEditBtn_);
    editLayout->addWidget(discardEditBtn_);
    editBanner_->hide();

    connect(saveEditBtn_, &QPushButton::clicked, this, &OrderPage::onSaveEdited);
    connect(discardEditBtn_, &QPushButton::clicked, this, &OrderPage::onDiscardEdit);

    // Top bar: Customer name (optional -> "Walk-in") and contact
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("Customer name (optional, default: Walk-in)");
    contactEdit_ = new QLineEdit(this);
    contactEdit_->setPlaceholderText("Contact number (optional)");

    auto* topLayout = new QHBoxLayout;
    topLayout->addWidget(new QLabel("Customer:", this));
    topLayout->addWidget(nameEdit_, 2);
    topLayout->addWidget(contactEdit_, 1);

    // Held Orders Strip (1-click resume buttons)
    auto* stripRow = new QHBoxLayout;
    auto* heldLabel = new QLabel("Held orders:", this);
    heldLabel->setStyleSheet("font-weight: bold; color: #374151;");
    stripRow->addWidget(heldLabel);

    heldStripHost_ = new QWidget(this);
    heldStripLayout_ = new QHBoxLayout(heldStripHost_);
    heldStripLayout_->setContentsMargins(0, 0, 0, 0);
    heldStripLayout_->setSpacing(6);
    noHeldLabel_ = new QLabel("None", heldStripHost_);
    noHeldLabel_->setStyleSheet("color: #6B7280; font-style: italic;");
    heldStripLayout_->addWidget(noHeldLabel_);
    heldStripLayout_->addStretch();

    auto* stripScroll = new QScrollArea(this);
    stripScroll->setWidget(heldStripHost_);
    stripScroll->setWidgetResizable(true);
    stripScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    stripScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    stripScroll->setFrameShape(QFrame::NoFrame);
    stripScroll->setFixedHeight(40);
    stripRow->addWidget(stripScroll, 1);

    // Left Panel: Camera + Manual Barcode + Quantity Box
    auto* leftLayout = new QVBoxLayout;
    cameraPanel_ = new CameraPanel(this);
    cameraPanel_->setMinimumSize(320, 240);
    connect(cameraPanel_, &CameraPanel::barcodeScanned, this, &OrderPage::addBarcode);
    connect(cameraPanel_, &CameraPanel::cameraMessage, this, [this](const QString& msg) {
        const bool ok = cameraPanel_ && cameraPanel_->isRunning();
        showResult({ok, msg.toStdString()});
    });

    leftLayout->addWidget(cameraPanel_);

    barcodeEdit_ = new QLineEdit(this);
    barcodeEdit_->setPlaceholderText("Scan or type 6-digit barcode + Enter");
    barcodeEdit_->setMaxLength(6);

    qtyBox_ = new QDoubleSpinBox(this);
    qtyBox_->setDecimals(3);
    qtyBox_->setRange(0.001, 100000.0);
    qtyBox_->setValue(1.0);

    auto* addBtn = new QPushButton("Add Item", this);
    addBtn->setStyleSheet("padding: 6px; font-weight: bold;");

    leftLayout->addWidget(new QLabel("Barcode:", this));
    leftLayout->addWidget(barcodeEdit_);
    leftLayout->addWidget(new QLabel("Quantity:", this));
    leftLayout->addWidget(qtyBox_);
    leftLayout->addWidget(addBtn);
    leftLayout->addStretch();

    connect(barcodeEdit_, &QLineEdit::returnPressed, this, &OrderPage::onManualAdd);
    connect(addBtn, &QPushButton::clicked, this, &OrderPage::onManualAdd);

    // Middle Panel: Cart Table
    cart_ = new QTableWidget(0, 5, this);
    cart_->setHorizontalHeaderLabels({"Item", "Name", "Qty", "Unit Price", "Line Total"});
    cart_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    cart_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cart_->setSelectionMode(QAbstractItemView::SingleSelection);
    cart_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(cart_, &QTableWidget::itemSelectionChanged, this, [this] { refresh(); });

    // Right Panel: Totals (from buildBill) + Action Buttons
    subtotalLbl_ = new QLabel("0.00 PKR", this);
    taxLbl_ = new QLabel("0.00 PKR", this);
    discountLbl_ = new QLabel("0.00 PKR", this);
    seasonalLbl_ = new QLabel("0.00 PKR", this);
    payableLbl_ = new QLabel("0.00 PKR", this);
    payableLbl_->setStyleSheet("font-size: 22px; font-weight: bold; color: #1e3a8a;");

    auto* formLayout = new QFormLayout;
    formLayout->addRow("Subtotal:", subtotalLbl_);
    formLayout->addRow("Tax:", taxLbl_);
    formLayout->addRow("Item Discount:", discountLbl_);
    formLayout->addRow("Seasonal Disc:", seasonalLbl_);
    formLayout->addRow("Payable:", payableLbl_);

    descBtn_ = new QPushButton("Description", this);
    plusBtn_ = new QPushButton("+ Qty", this);
    minusBtn_ = new QPushButton("- Qty", this);
    removeBtn_ = new QPushButton("Remove", this);
    holdBtn_ = new QPushButton("Hold (F2)", this);
    openOrderBtn_ = new QPushButton("Open Order", this);
    payBtn_ = new QPushButton("Pay (F4)", this);
    payBtn_->setStyleSheet("background-color: #15803d; color: white; font-weight: bold; padding: 8px;");
    cancelBtn_ = new QPushButton("Cancel Order", this);
    cancelBtn_->setStyleSheet("background-color: #b91c1c; color: white; padding: 6px;");

    auto* rightLayout = new QVBoxLayout;
    rightLayout->addLayout(formLayout);
    for (auto* btn : {descBtn_, plusBtn_, minusBtn_, removeBtn_, holdBtn_, openOrderBtn_, payBtn_, cancelBtn_}) {
        rightLayout->addWidget(btn);
    }
    rightLayout->addStretch();

    connect(descBtn_, &QPushButton::clicked, this, &OrderPage::onDescription);
    connect(plusBtn_, &QPushButton::clicked, this, &OrderPage::onPlus);
    connect(minusBtn_, &QPushButton::clicked, this, &OrderPage::onMinus);
    connect(removeBtn_, &QPushButton::clicked, this, &OrderPage::onRemove);
    connect(holdBtn_, &QPushButton::clicked, this, &OrderPage::onHold);
    connect(openOrderBtn_, &QPushButton::clicked, this, &OrderPage::onOpenOrder);
    connect(payBtn_, &QPushButton::clicked, this, &OrderPage::onPay);
    connect(cancelBtn_, &QPushButton::clicked, this, &OrderPage::onCancel);

    // Keyboard shortcuts: F2 = Hold, F4 = Pay
    auto* holdShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(holdShortcut, &QShortcut::activated, this, [this] {
        if (holdBtn_->isEnabled()) onHold();
    });
    auto* payShortcut = new QShortcut(QKeySequence(Qt::Key_F4), this);
    connect(payShortcut, &QShortcut::activated, this, [this] {
        if (payBtn_->isEnabled()) onPay();
    });

    // Status Banner at bottom
    banner_ = new QLabel(this);
    banner_->setWordWrap(true);
    banner_->setMinimumHeight(32);
    banner_->setStyleSheet("background:#f3f4f6; color:#374151; padding: 6px; font-weight: bold;");

    auto* bodyLayout = new QHBoxLayout;
    bodyLayout->addLayout(leftLayout, 3);
    bodyLayout->addWidget(cart_, 5);
    bodyLayout->addLayout(rightLayout, 2);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addWidget(editBanner_);
    rootLayout->addLayout(topLayout);
    rootLayout->addLayout(stripRow);
    rootLayout->addLayout(bodyLayout, 1);
    rootLayout->addWidget(banner_);

    barcodeEdit_->setFocus();
}

void OrderPage::showResult(const OperationResult& r) {
    const QString msg = QString::fromStdString(r.message);
    banner_->setText(msg);
    banner_->setStyleSheet(r.success
        ? "background:#e6f4ea; color:#1e4620; padding:6px; font-weight:bold; border: 1px solid #a3cfbb;"
        : "background:#fdecea; color:#7a1c1c; padding:6px; font-weight:bold; border: 1px solid #f1aeb5;");
    emit statusMessage(msg, r.success);
}

QString OrderPage::selectedProductId() const {
    const int row = cart_->currentRow();
    if (row < 0 || !cart_->item(row, 0)) return {};
    return cart_->item(row, 0)->data(Qt::UserRole).toString();
}

void OrderPage::updateEditBanner(const std::optional<Order>& originalOrder) {
    editingPaidOrder_ = originalOrder.has_value();
    if (originalOrder.has_value()) {
        editBannerLabel_->setText(
            QString("Editing paid order %1").arg(QString::fromStdString(originalOrder->id)));
        editBanner_->show();
    } else {
        editBanner_->hide();
    }
}

void OrderPage::refreshHeldStrip() {
    while (heldStripLayout_->count() > 0) {
        QLayoutItem* item = heldStripLayout_->takeAt(0);
        if (QWidget* w = item->widget()) {
            if (w == noHeldLabel_) {
                w->hide();
            } else {
                w->deleteLater();
            }
        }
        delete item;
    }

    const std::vector<Order> held = orders_.heldOrders();
    if (held.empty()) {
        noHeldLabel_->show();
        heldStripLayout_->addWidget(noHeldLabel_);
        heldStripLayout_->addStretch();
        return;
    }

    noHeldLabel_->hide();
    for (const Order& ord : held) {
        const QString cust = ord.customerName.empty()
            ? QString("Walk-in")
            : QString::fromStdString(ord.customerName);
        const QString label = QString("#%1 · %2 (%3)")
            .arg(QString::fromStdString(ord.id))
            .arg(cust)
            .arg(ord.lines.size());
        auto* btn = new QPushButton(label, heldStripHost_);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: #EEF2FF; color: #1E3A8A; border: 1px solid #C7D2FE;"
            " border-radius: 4px; padding: 4px 10px; font-weight: bold; }"
            "QPushButton:hover { background: #E0E7FF; }");
        const std::string orderId = ord.id;
        connect(btn, &QPushButton::clicked, this, [this, orderId] {
            onResumeHeld(orderId);
        });
        heldStripLayout_->addWidget(btn);
    }
    heldStripLayout_->addStretch();
}

void OrderPage::refresh() {
    const QString keepId = selectedProductId();
    const auto& cur = orders_.current();
    const bool hasLines = cur && !cur->lines.empty();

    updateEditBanner(orders_.original());

    QSignalBlocker block(cart_);
    cart_->setRowCount(0);

    if (hasLines) {
        // Totals come strictly from buildBill() (never computed in GUI).
        std::string priceDate = todayText();
        if (editingPaidOrder_ && orders_.original().has_value()) {
            if (const auto origBill = billStore_.findByOrderId(orders_.original()->id)) {
                priceDate = origBill->date;
            } else if (!orders_.original()->date.empty()) {
                priceDate = orders_.original()->date;
            }
        }
        BillOptions draftOpts;
        draftOpts.date = priceDate;
        draftOpts.time = nowTimeText();
        draftOpts.seasonalPercent = seasons_.percentFor(priceDate);
        Bill draft = buildBill(*cur, catalog_, draftOpts);

        int row = 0;
        for (const auto& orderLine : cur->lines) {
            std::optional<Product> p = catalog_.findById(orderLine.productId);
            if (!p && !orderLine.barcode.empty()) {
                p = catalog_.findByBarcode(orderLine.barcode);
            }
            if (!p) continue;
            const Money lineSubtotal = multiplyPriceByQuantity(p->unitPrice, orderLine.quantity);
            cart_->insertRow(row);
            const QStringList cells = {
                QString::fromStdString(p->acronym),
                QString::fromStdString(p->name),
                qtyText(orderLine.quantity, p->unit),
                moneyText(p->unitPrice),
                moneyText(lineSubtotal)
            };
            for (int c = 0; c < 5; ++c) {
                auto* it = new QTableWidgetItem(cells[c]);
                if (c >= 2) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                if (c == 0) it->setData(Qt::UserRole, QString::fromStdString(p->id));
                cart_->setItem(row, c, it);
            }
            if (QString::fromStdString(p->id) == keepId) cart_->selectRow(row);
            ++row;
        }

        subtotalLbl_->setText(moneyText(draft.subtotal));
        taxLbl_->setText(moneyText(draft.tax));
        discountLbl_->setText(moneyText(draft.itemDiscount));
        seasonalLbl_->setText(moneyText(draft.seasonalDiscount));
        payableLbl_->setText(moneyText(draft.payable));
    } else {
        subtotalLbl_->setText("0.00 PKR");
        taxLbl_->setText("0.00 PKR");
        discountLbl_->setText("0.00 PKR");
        seasonalLbl_->setText("0.00 PKR");
        payableLbl_->setText("0.00 PKR");
    }

    refreshHeldStrip();

    const bool canRecord = isAllowed(role_, Action::RecordOrder);
    const bool canViewProd = isAllowed(role_, Action::ViewProduct);
    const bool canOpen = isAllowed(role_, Action::OpenOrder);
    const bool sel = !selectedProductId().isEmpty();

    descBtn_->setEnabled(canViewProd && hasLines);
    plusBtn_->setEnabled(canRecord && sel);
    minusBtn_->setEnabled(canRecord && sel);
    removeBtn_->setEnabled(canRecord && sel);
    holdBtn_->setEnabled(canRecord && hasLines);
    openOrderBtn_->setEnabled(canOpen);
    payBtn_->setEnabled(canRecord && hasLines && !editingPaidOrder_);
    cancelBtn_->setEnabled(canRecord && cur.has_value());
    nameEdit_->setEnabled(canRecord && !cur.has_value());
    contactEdit_->setEnabled(canRecord && !cur.has_value());
}

void OrderPage::addBarcode(const QString& code) {
    if (!isAllowed(role_, Action::RecordOrder)) {
        showResult({false, "Current role is not allowed to record orders."});
        return;
    }

    if (!orders_.current().has_value()) {
        QString custName = nameEdit_->text().trimmed();
        if (custName.isEmpty()) custName = "Walk-in";
        auto r = orders_.startOrder(custName.toStdString(),
                                    contactEdit_->text().trimmed().toStdString());
        if (!r.success) {
            showResult(r);
            return;
        }
    }

    const Quantity q = std::llround(qtyBox_->value() * 1000.0);
    auto r = orders_.addByBarcode(code.toStdString(), q);
    if (r.success) {
        lastBarcode_ = code;
        qtyBox_->setValue(1.0);
    }
    showResult(r);
    refresh();
}

void OrderPage::onManualAdd() {
    const QString code = barcodeEdit_->text().trimmed();
    if (code.isEmpty()) return;
    addBarcode(code);
    barcodeEdit_->clear();
    barcodeEdit_->setFocus();
}

void OrderPage::onDescription() {
    if (!isAllowed(role_, Action::ViewProduct)) {
        showResult({false, "Current role is not allowed to view product details."});
        return;
    }

    std::optional<Product> p;
    const QString id = selectedProductId();
    if (!id.isEmpty()) {
        p = catalog_.findById(id.toStdString());
    } else if (!lastBarcode_.isEmpty()) {
        p = catalog_.findByBarcode(lastBarcode_.toStdString());
    }

    if (!p) {
        showResult({false, "No product selected to describe."});
        return;
    }

    ProductInfoDialog dlg(*p, this);
    dlg.exec();
}

void OrderPage::changeSelectedQuantity(int direction) {
    const QString id = selectedProductId();
    const auto& cur = orders_.current();
    if (id.isEmpty() || !cur) return;
    const auto p = catalog_.findById(id.toStdString());
    if (!p) return;

    const Quantity step = (p->unit == MeasureUnit::Weight) ? 100 : 1000;
    for (const auto& line : cur->lines) {
        if (QString::fromStdString(line.productId) != id) continue;
        const Quantity next = line.quantity + direction * step;
        auto r = (next <= 0) ? orders_.removeLine(id.toStdString())
                             : orders_.setQuantity(id.toStdString(), next);
        showResult(r);
        break;
    }
    refresh();
}

void OrderPage::onPlus()  { changeSelectedQuantity(+1); }
void OrderPage::onMinus() { changeSelectedQuantity(-1); }

void OrderPage::onRemove() {
    const QString id = selectedProductId();
    if (id.isEmpty()) return;
    showResult(orders_.removeLine(id.toStdString()));
    refresh();
}

void OrderPage::onHold() {
    if (!isAllowed(role_, Action::RecordOrder)) {
        showResult({false, "Current role is not allowed to hold orders."});
        return;
    }
    OperationResult r = orders_.hold();
    if (r.success) {
        nameEdit_->clear();
        contactEdit_->clear();
    }
    showResult(r);
    refresh();
    barcodeEdit_->setFocus();
}

void OrderPage::onResumeHeld(const std::string& orderId) {
    if (!isAllowed(role_, Action::RecordOrder)) {
        showResult({false, "Current role is not allowed to resume orders."});
        return;
    }

    // Until M3's OrderManager::resume() auto-holds a non-empty current order (and drops an empty one),
    // preserve that exact one-click behavior here.
    if (const auto& cur = orders_.current(); cur.has_value()) {
        OperationResult pre = cur->lines.empty() ? orders_.cancel() : orders_.hold();
        if (!pre.success) {
            showResult(pre);
            return;
        }
    }

    OperationResult r = orders_.resume(orderId);
    if (r.success && orders_.current().has_value()) {
        nameEdit_->setText(QString::fromStdString(orders_.current()->customerName));
        contactEdit_->setText(QString::fromStdString(orders_.current()->customerContact));
    }
    showResult(r);
    refresh();
    barcodeEdit_->setFocus();
}

void OrderPage::onOpenOrder() {
    if (!isAllowed(role_, Action::OpenOrder)) {
        showResult({false, "Current role is not allowed to open orders."});
        return;
    }

    const bool needsApproval = needsManagerApproval(role_, Action::EditPaidOrder);
    auto approvalFn = [this]() -> bool {
        return ApprovalDialog::ask(auth_, this, "Approve opening/editing a paid order");
    };

    OpenOrderDialog dlg(orders_, orderStore_, billStore_, todayText(), approvalFn, needsApproval, this);
    if (dlg.exec() == QDialog::Accepted && orders_.current().has_value()) {
        nameEdit_->setText(QString::fromStdString(orders_.current()->customerName));
        contactEdit_->setText(QString::fromStdString(orders_.current()->customerContact));
        showResult({true, "Opened order " + orders_.current()->id + "."});
    }
    refresh();
    barcodeEdit_->setFocus();
}

void OrderPage::onPay() {
    if (!isAllowed(role_, Action::RecordOrder)) {
        showResult({false, "Current role is not allowed to take payments."});
        return;
    }
    const auto& cur = orders_.current();
    if (!cur || cur->lines.empty()) {
        showResult({false, "No active order to pay."});
        return;
    }

    BillOptions opts;
    opts.date = cur->date.empty() ? todayText() : cur->date;
    opts.time = cur->time.empty() ? nowTimeText() : cur->time;
    opts.seasonalPercent = seasons_.percentFor(opts.date);
    Bill draft = buildBill(*cur, catalog_, opts);

    const bool canSpecialDirect = !needsManagerApproval(role_, Action::ApplySpecialDiscount);
    auto approvalFn = [this]() -> bool {
        return ApprovalDialog::ask(auth_, this, "Approve special discount (up to 8.00%)");
    };
    PaymentDialog dlg(draft.payable, canSpecialDirect, approvalFn, this);
    if (dlg.exec() == QDialog::Accepted) {
        PaymentInfo payment = dlg.paymentInfo();
        Bill finalBill;
        auto r = checkout_.pay(orders_, payment, finalBill);
        showResult(r);
        if (r.success) {
            nameEdit_->clear();
            contactEdit_->clear();
            BillDialog billDlg(finalBill, this);
            billDlg.exec();
        }
        refresh();
        barcodeEdit_->setFocus();
    }
}

void OrderPage::onCancel() {
    if (QMessageBox::question(this, "Cancel order",
                              "Are you sure you want to cancel this order?") != QMessageBox::Yes) {
        return;
    }
    if (editingPaidOrder_) {
        onDiscardEdit();
        return;
    }
    OperationResult r = orders_.cancel();
    if (r.success) {
        nameEdit_->clear();
        contactEdit_->clear();
    }
    showResult(r);
    refresh();
    barcodeEdit_->setFocus();
}

void OrderPage::onSaveEdited() {
    const auto& cur = orders_.current();
    const auto& orig = orders_.original();
    if (!cur || !orig) {
        showResult({false, "No paid order is currently being edited."});
        return;
    }
    if (cur->lines.empty()) {
        showResult({false, "Cannot save an empty edited order."});
        return;
    }

    std::string billDate = orig->date;
    if (const auto origBill = billStore_.findByOrderId(orig->id)) {
        billDate = origBill->date;
    }
    if (billDate.empty()) {
        billDate = todayText();
    }

    BillOptions opts;
    opts.date = billDate;
    opts.time = orig->time;
    opts.seasonalPercent = seasons_.percentFor(billDate);
    Bill draft = buildBill(*cur, catalog_, opts);

    const bool canSpecialDirect = !needsManagerApproval(role_, Action::ApplySpecialDiscount);
    auto approvalFn = [this]() -> bool {
        return ApprovalDialog::ask(auth_, this, "Approve special discount (up to 8.00%)");
    };
    PaymentDialog dlg(draft.payable, canSpecialDirect, approvalFn, this);
    if (dlg.exec() == QDialog::Accepted) {
        PaymentInfo payment = dlg.paymentInfo();
        Bill finalBill;
        OperationResult r = checkout_.saveEdited(orders_, payment, finalBill);
        showResult(r);
        if (r.success) {
            nameEdit_->clear();
            contactEdit_->clear();
            BillDialog billDlg(finalBill, this);
            billDlg.exec();
        }
        refresh();
        barcodeEdit_->setFocus();
    }
}

void OrderPage::onDiscardEdit() {
    OperationResult r = OrderEditService::discard(orders_);
    if (r.success) {
        nameEdit_->clear();
        contactEdit_->clear();
    }
    showResult(r);
    refresh();
    barcodeEdit_->setFocus();
}
