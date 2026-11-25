#include "CategoriesPage.h"
#include "DialogStyle.h"

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

CategoriesPage::CategoriesPage(CategoryStore& store, const std::string& path, QWidget* parent)
    : QWidget(parent), store_(store), path_(path)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(DialogStyle::dialogStyleSheet());

    table_ = new QTableWidget(0, 3);
    table_->setHorizontalHeaderLabels({"ID", "Name", "Acronym"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);

    id_ = new QLineEdit;      id_->setPlaceholderText("ID (10-99)");
    name_ = new QLineEdit;    name_->setPlaceholderText("Category name");
    acronym_ = new QLineEdit; acronym_->setPlaceholderText("Acronym (max 8)");

    auto* addBtn = new QPushButton("Add category");
    addBtn->setObjectName("confirmBtn");
    auto* renameBtn = new QPushButton("Rename selected");
    renameBtn->setObjectName("closeBtn");

    status_ = new QLabel;

    auto* form = new QHBoxLayout;
    form->addWidget(id_, 1);
    form->addWidget(name_, 3);
    form->addWidget(acronym_, 2);
    form->addWidget(addBtn);
    form->addWidget(renameBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table_, 1);
    layout->addLayout(form);
    layout->addWidget(status_);

    connect(addBtn, &QPushButton::clicked, this, &CategoriesPage::onAdd);
    connect(renameBtn, &QPushButton::clicked, this, &CategoriesPage::onRename);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &CategoriesPage::onRowSelected);

    refresh();
}

void CategoriesPage::refresh()
{
    const auto items = store_.all();
    table_->setRowCount(static_cast<int>(items.size()));
    for (int r = 0; r < static_cast<int>(items.size()); ++r) {
        table_->setItem(r, 0, new QTableWidgetItem(qs(items[r].id)));
        table_->setItem(r, 1, new QTableWidgetItem(qs(items[r].name)));
        table_->setItem(r, 2, new QTableWidgetItem(qs(items[r].acronym)));
    }
}

void CategoriesPage::showMessage(const QString& text, bool ok)
{
    status_->setStyleSheet(QString("color: %1; font-weight: bold;")
                               .arg(ok ? DialogStyle::kSuccess : DialogStyle::kDanger));
    status_->setText(text);
}

void CategoriesPage::onRowSelected()
{
    const int row = table_->currentRow();
    if (row < 0) return;
    id_->setText(table_->item(row, 0)->text());
    name_->setText(table_->item(row, 1)->text());
    acronym_->setText(table_->item(row, 2)->text());
}

void CategoriesPage::onAdd()
{
    Category c{id_->text().trimmed().toStdString(),
               name_->text().trimmed().toStdString(),
               acronym_->text().trimmed().toStdString()};
    OperationResult r = store_.add(c);
    if (!r.success) { showMessage(qs(r.message), false); return; }

    OperationResult s = store_.save(path_);
    if (!s.success) { showMessage("Added, but could not save: " + qs(s.message), false); }
    else showMessage(qs(r.message), true);

    refresh();
    id_->clear(); name_->clear(); acronym_->clear();
    emit categoriesChanged();
}

void CategoriesPage::onRename()
{
    Category c{id_->text().trimmed().toStdString(),
               name_->text().trimmed().toStdString(),
               acronym_->text().trimmed().toStdString()};
    OperationResult r = store_.update(c);     // id stays, name/acronym change
    if (!r.success) { showMessage(qs(r.message), false); return; }

    OperationResult s = store_.save(path_);
    if (!s.success) { showMessage("Updated, but could not save: " + qs(s.message), false); }
    else showMessage(qs(r.message), true);

    refresh();
    emit categoriesChanged();
}