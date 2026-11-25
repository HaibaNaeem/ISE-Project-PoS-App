// OWNER: Member 3. Manager inventory screen: view stock, search, set or receive stock.
#include "InventoryPage.h"
#include "Units.h"
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QVBoxLayout>
#include <optional>

InventoryPage::InventoryPage(InventoryService& inventory, QWidget* parent)
    : QWidget(parent), inventory_(inventory) {
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(tr("Search by ID, barcode, acronym, or name..."));

    table_ = new QTableWidget(0, 7, this);
    table_->setHorizontalHeaderLabels(
        {tr("ID"), tr("Barcode"), tr("Category"), tr("Acronym"), tr("Name"), tr("Unit"), tr("Available")});
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    qtyEdit_ = new QLineEdit(this);
    qtyEdit_->setPlaceholderText(tr("Quantity (weight: up to 3 decimals)"));
    qtyEdit_->setMaximumWidth(240);

    setButton_ = new QPushButton(tr("Set Stock"), this);
    receiveButton_ = new QPushButton(tr("Receive Stock"), this);
    setButton_->setEnabled(false);
    receiveButton_->setEnabled(false);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setMinimumHeight(28);
    statusLabel_->setStyleSheet("background:#f3f4f6; color:#374151; padding:6px;");

    auto* topBar = new QHBoxLayout;
    topBar->addWidget(new QLabel(tr("Search:"), this));
    topBar->addWidget(searchEdit_, 1);

    auto* actionBar = new QHBoxLayout;
    actionBar->addWidget(new QLabel(tr("Quantity:"), this));
    actionBar->addWidget(qtyEdit_);
    actionBar->addWidget(setButton_);
    actionBar->addWidget(receiveButton_);
    actionBar->addStretch(1);

    auto* root = new QVBoxLayout(this);
    root->addLayout(topBar);
    root->addWidget(table_, 1);
    root->addLayout(actionBar);
    root->addWidget(statusLabel_);

    connect(searchEdit_, &QLineEdit::textChanged, this, &InventoryPage::onSearchChanged);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &InventoryPage::onSelectionChanged);
    connect(setButton_, &QPushButton::clicked, this, &InventoryPage::onSetStock);
    connect(receiveButton_, &QPushButton::clicked, this, &InventoryPage::onReceiveStock);

    refresh();
}

void InventoryPage::refresh() {
    const QString keepId = selectedProductId();
    const QString filter = searchEdit_->text().trimmed();

    QSignalBlocker blocker(table_);
    table_->setRowCount(0);

    int row = 0;
    for (const Product& p : inventory_.list()) {
        const QString idStr = QString::fromStdString(p.id);
        const QString barcodeStr = QString::fromStdString(p.barcode);
        const QString acronymStr = QString::fromStdString(p.acronym);
        const QString nameStr = QString::fromStdString(p.name);

        if (!filter.isEmpty() &&
            !idStr.contains(filter, Qt::CaseInsensitive) &&
            !barcodeStr.contains(filter, Qt::CaseInsensitive) &&
            !acronymStr.contains(filter, Qt::CaseInsensitive) &&
            !nameStr.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        // TODO(M2): display category name/id from Product::categoryId / CategoryStore when merged.
        const QString categoryStr = tr("-");
        const QString unitStr = QString::fromStdString(measureUnitToText(p.unit));
        const QString availStr = QString::fromStdString(formatQuantity(p.available, p.unit));

        table_->insertRow(row);
        const QStringList cells = {idStr, barcodeStr, categoryStr, acronymStr, nameStr, unitStr, availStr};
        const bool isZeroStock = (p.available == 0);

        for (int col = 0; col < cells.size(); ++col) {
            auto* item = new QTableWidgetItem(cells[col]);
            if (col == 0) {
                item->setData(Qt::UserRole, idStr);
            }
            if (col == 6) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
            if (isZeroStock) {
                item->setForeground(QBrush(QColor("#DC2626")));
                item->setBackground(QBrush(QColor("#FEE2E2")));
                if (col == 6) {
                    QFont f = item->font();
                    f.setBold(true);
                    item->setFont(f);
                }
            }
            table_->setItem(row, col, item);
        }

        if (idStr == keepId) {
            table_->selectRow(row);
        }
        ++row;
    }

    onSelectionChanged();
}

void InventoryPage::onSearchChanged(const QString&) {
    refresh();
}

void InventoryPage::onSelectionChanged() {
    const bool hasSelection = !selectedProductId().isEmpty();
    setButton_->setEnabled(hasSelection);
    receiveButton_->setEnabled(hasSelection);
}

void InventoryPage::onSetStock() {
    applyAction(false);
}

void InventoryPage::onReceiveStock() {
    applyAction(true);
}

void InventoryPage::applyAction(bool isReceive) {
    const QString prodId = selectedProductId();
    if (prodId.isEmpty()) {
        showStatus({false, "Please select a product first."});
        return;
    }

    std::optional<Product> selected;
    for (const Product& p : inventory_.list()) {
        if (QString::fromStdString(p.id) == prodId) {
            selected = p;
            break;
        }
    }
    if (!selected.has_value()) {
        showStatus({false, "Selected product no longer exists."});
        return;
    }

    QString qtyText = qtyEdit_->text().trimmed();
    if (qtyText.isEmpty()) {
        bool ok = false;
        const QString title = isReceive ? tr("Receive Stock") : tr("Set Stock");
        const QString prompt = tr("Enter quantity for %1 (%2):")
                                   .arg(QString::fromStdString(selected->name),
                                        QString::fromStdString(measureUnitToText(selected->unit)));
        qtyText = QInputDialog::getText(this, title, prompt, QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok) {
            return;
        }
    }

    Quantity qty = 0;
    if (qtyText.startsWith('-')) {
        qty = -1000; // Let InventoryService return the core negative-quantity refusal message
    } else {
        const OperationResult parsed = parseQuantity(qtyText.toStdString(), selected->unit, qty);
        if (!parsed.success) {
            showStatus(parsed);
            return;
        }
    }

    const OperationResult res = isReceive
                                    ? inventory_.receiveStock(selected->id, qty)
                                    : inventory_.setAvailable(selected->id, qty);
    showStatus(res);
    if (res.success) {
        qtyEdit_->clear();
        refresh();
    }
}

QString InventoryPage::selectedProductId() const {
    const int row = table_->currentRow();
    if (row < 0 || !table_->item(row, 0)) {
        return {};
    }
    return table_->item(row, 0)->data(Qt::UserRole).toString();
}

void InventoryPage::showStatus(const OperationResult& result) {
    statusLabel_->setText(QString::fromStdString(result.message));
    statusLabel_->setStyleSheet(result.success
                                    ? "background:#e6f4ea; color:#1e4620; padding:6px; font-weight:bold; border:1px solid #a3cfbb;"
                                    : "background:#fdecea; color:#7a1c1c; padding:6px; font-weight:bold; border:1px solid #f1aeb5;");
}
