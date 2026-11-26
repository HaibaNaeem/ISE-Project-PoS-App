// OWNER: Member 5. Manager screen: view, add, and activate/deactivate seasonal discounts.
#include "SeasonsPage.h"
#include "DateText.h"
#include "DialogStyle.h"
#include "Units.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

SeasonsPage::SeasonsPage(SeasonStore& store, std::string seasonsPath, QWidget* parent)
    : QWidget(parent), store_(store), seasonsPath_(std::move(seasonsPath)) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(DialogStyle::dialogStyleSheet() +
                  "SeasonsPage { background-color: #F5F7FA; color: #1F2937; }");

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"Start Date", "End Date", "Discount (%)", "Active"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    const QString today = QString::fromStdString(todayText());
    startDateEdit_ = new QLineEdit(today, this);
    startDateEdit_->setPlaceholderText("Start (YYYY-MM-DD)");
    endDateEdit_ = new QLineEdit(today, this);
    endDateEdit_->setPlaceholderText("End (YYYY-MM-DD)");
    percentEdit_ = new QLineEdit(this);
    percentEdit_->setPlaceholderText("Percent (e.g. 10.00)");

    activeCheck_ = new QCheckBox("Active", this);
    activeCheck_->setChecked(true);

    auto* addBtn = new QPushButton("Add Season", this);
    addBtn->setObjectName("confirmBtn");

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);

    auto* form = new QHBoxLayout;
    form->addWidget(new QLabel("Start:", this));
    form->addWidget(startDateEdit_, 2);
    form->addWidget(new QLabel("End:", this));
    form->addWidget(endDateEdit_, 2);
    form->addWidget(new QLabel("Percent:", this));
    form->addWidget(percentEdit_, 2);
    form->addWidget(activeCheck_);
    form->addWidget(addBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(table_, 1);
    layout->addLayout(form);
    layout->addWidget(statusLabel_);

    connect(addBtn, &QPushButton::clicked, this, &SeasonsPage::onAdd);
    connect(table_, &QTableWidget::itemChanged, this, &SeasonsPage::onItemChanged);

    refresh();
}

void SeasonsPage::refresh() {
    QSignalBlocker blocker(table_);
    const auto& seasons = store_.all();
    table_->setRowCount(static_cast<int>(seasons.size()));

    for (int r = 0; r < static_cast<int>(seasons.size()); ++r) {
        const Season& s = seasons[r];
        auto* startItem = new QTableWidgetItem(QString::fromStdString(s.startDate));
        auto* endItem = new QTableWidgetItem(QString::fromStdString(s.endDate));
        auto* pctItem = new QTableWidgetItem(QString::fromStdString(formatPercent(s.percent)) + "%");
        pctItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* activeItem = new QTableWidgetItem(s.active ? "Yes" : "No");
        activeItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        activeItem->setCheckState(s.active ? Qt::Checked : Qt::Unchecked);

        table_->setItem(r, 0, startItem);
        table_->setItem(r, 1, endItem);
        table_->setItem(r, 2, pctItem);
        table_->setItem(r, 3, activeItem);
    }
}

void SeasonsPage::onAdd() {
    Percent pct = 0;
    const std::string pctText = percentEdit_->text().trimmed().toStdString();
    OperationResult pctRes = parsePercent(pctText, pct);
    if (!pctRes.success) {
        showStatus(QString::fromStdString(pctRes.message), false);
        return;
    }

    Season s;
    s.startDate = startDateEdit_->text().trimmed().toStdString();
    s.endDate = endDateEdit_->text().trimmed().toStdString();
    s.percent = pct;
    s.active = activeCheck_->isChecked();

    OperationResult addRes = store_.add(s);
    if (!addRes.success) {
        showStatus(QString::fromStdString(addRes.message), false);
        return;
    }

    OperationResult saveRes = store_.save(seasonsPath_);
    if (!saveRes.success) {
        showStatus("Added in memory, but save failed: " + QString::fromStdString(saveRes.message), false);
    } else {
        showStatus(QString::fromStdString(addRes.message), true);
        percentEdit_->clear();
    }

    refresh();
    emit seasonsChanged();
}

void SeasonsPage::onItemChanged(QTableWidgetItem* item) {
    if (!item || item->column() != 3) return;
    const int row = item->row();
    if (row < 0 || static_cast<std::size_t>(row) >= store_.all().size()) return;

    const bool nowActive = (item->checkState() == Qt::Checked);
    OperationResult res = store_.setActive(static_cast<std::size_t>(row), nowActive);
    if (!res.success) {
        showStatus(QString::fromStdString(res.message), false);
        refresh();
        return;
    }

    {
        QSignalBlocker blocker(table_);
        item->setText(nowActive ? "Yes" : "No");
    }

    OperationResult saveRes = store_.save(seasonsPath_);
    if (!saveRes.success) {
        showStatus("Updated, but save failed: " + QString::fromStdString(saveRes.message), false);
    } else {
        showStatus(QString::fromStdString(res.message), true);
    }

    emit seasonsChanged();
}

void SeasonsPage::showStatus(const QString& text, bool ok) {
    statusLabel_->setStyleSheet(QString("color: %1; font-weight: bold;")
                                    .arg(ok ? DialogStyle::kSuccess : DialogStyle::kDanger));
    statusLabel_->setText(text);
}
