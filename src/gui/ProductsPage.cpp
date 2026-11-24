#include "ProductsPage.h"
#include "DialogStyle.h"
#include "ProductEditorDialog.h"
#include "ProductInfoDialog.h"
#include "Units.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString qs(const std::string& s) { return QString::fromStdString(s); }
}

ProductsPage::ProductsPage(ProductCatalog& catalog, CategoryStore& categories,
                           const std::string& productPath, QWidget* parent)
    : QWidget(parent), catalog_(catalog), categories_(categories), path_(productPath)
{
    setStyleSheet(DialogStyle::dialogStyleSheet());

    table_ = new QTableWidget(0, 8);
    table_->setHorizontalHeaderLabels({"ID", "Barcode", "Category", "Acronym", "Name", "Unit", "Price", "In stock"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);

    search_ = new QLineEdit;
    search_->setPlaceholderText("Type or scan a barcode to find a product");

    auto* addBtn = new QPushButton("Add product");
    addBtn->setObjectName("confirmBtn");
    auto* editBtn = new QPushButton("Edit selected");
    editBtn->setObjectName("closeBtn");
    auto* detailsBtn = new QPushButton("Details");
    detailsBtn->setObjectName("closeBtn");

    status_ = new QLabel;

    auto* top = new QHBoxLayout;
    top->addWidget(search_, 1);
    top->addWidget(addBtn);
    top->addWidget(editBtn);
    top->addWidget(detailsBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(table_, 1);
    layout->addWidget(status_);

    connect(addBtn, &QPushButton::clicked, this, &ProductsPage::onAdd);
    connect(editBtn, &QPushButton::clicked, this, &ProductsPage::onEdit);
    connect(detailsBtn, &QPushButton::clicked, this, &ProductsPage::onDetails);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) { onEdit(); });
    connect(search_, &QLineEdit::textChanged, this, &ProductsPage::onSearch);

    refresh();
}

void ProductsPage::refresh()
{
    const auto& items = catalog_.all();
    table_->setRowCount(static_cast<int>(items.size()));
    for (int r = 0; r < static_cast<int>(items.size()); ++r) {
        const Product& p = items[r];
        table_->setItem(r, 0, new QTableWidgetItem(qs(p.id)));
        table_->setItem(r, 1, new QTableWidgetItem(qs(p.barcode)));
        table_->setItem(r, 2, new QTableWidgetItem(qs(p.categoryId)));
        table_->setItem(r, 3, new QTableWidgetItem(qs(p.acronym)));
        table_->setItem(r, 4, new QTableWidgetItem(qs(p.name)));
        table_->setItem(r, 5, new QTableWidgetItem(qs(measureUnitToText(p.unit))));
        table_->setItem(r, 6, new QTableWidgetItem(qs(formatMoney(p.unitPrice))));
        table_->setItem(r, 7, new QTableWidgetItem(qs(formatQuantity(p.available, p.unit))));
    }
    status_->setText(QString::number(items.size()) + " products");
}

const Product* ProductsPage::selectedProduct() const
{
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(catalog_.all().size())) return nullptr;
    return &catalog_.all()[row];
}

void ProductsPage::onAdd()
{
    ProductEditorDialog dlg(catalog_, categories_, path_, nullptr, this);
    if (dlg.exec() == QDialog::Accepted) refresh();
}

void ProductsPage::onEdit()
{
    const Product* p = selectedProduct();
    if (!p) { status_->setText("Select a product first."); return; }
    Product copy = *p;      // dialog may change the catalog, so don't keep the pointer
    ProductEditorDialog dlg(catalog_, categories_, path_, &copy, this);
    if (dlg.exec() == QDialog::Accepted) refresh();
}

void ProductsPage::onDetails()
{
    const Product* p = selectedProduct();
    if (!p) { status_->setText("Select a product first."); return; }
    ProductInfoDialog dlg(*p, this);
    dlg.exec();
}

void ProductsPage::onSearch(const QString& text)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return;
    for (int r = 0; r < table_->rowCount(); ++r) {
        if (table_->item(r, 1)->text() == t) {
            table_->selectRow(r);
            table_->scrollToItem(table_->item(r, 1));
            status_->setText("Found: " + table_->item(r, 4)->text());
            return;
        }
    }
    status_->setText("No product with this barcode.");
}