// OWNER: Member 5. Manager screen: read-only reports powered by ReportRegistry.
#include "ReportsPage.h"
#include "DateText.h"
#include "DialogStyle.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

ReportsPage::ReportsPage(const ReportRegistry& registry,
                         const ProductCatalog& catalog,
                         const OrderStore& orders,
                         const BillStore& bills,
                         QWidget* parent)
    : QWidget(parent),
      registry_(registry),
      catalog_(catalog),
      orders_(orders),
      bills_(bills) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(DialogStyle::dialogStyleSheet() +
                  "ReportsPage { background-color: #F5F7FA; color: #1F2937; }");

    reportCombo_ = new QComboBox(this);
    for (const auto& report : registry_.all()) {
        reportCombo_->addItem(QString::fromStdString(report->name()));
    }

    periodCombo_ = new QComboBox(this);
    periodCombo_->addItems({"Today", "Last 7 Days", "This Month", "This Year", "Custom"});

    const QDate today = QDate::fromString(QString::fromStdString(todayText()), "yyyy-MM-dd");
    fromDateEdit_ = new QDateEdit(today.isValid() ? today : QDate::currentDate(), this);
    fromDateEdit_->setDisplayFormat("yyyy-MM-dd");
    fromDateEdit_->setCalendarPopup(true);

    toDateEdit_ = new QDateEdit(today.isValid() ? today : QDate::currentDate(), this);
    toDateEdit_->setDisplayFormat("yyyy-MM-dd");
    toDateEdit_->setCalendarPopup(true);

    refreshBtn_ = new QPushButton("Refresh", this);
    refreshBtn_->setObjectName("closeBtn");

    titleLabel_ = new QLabel(this);
    titleLabel_->setStyleSheet("font-size: 14pt; font-weight: bold; color: #0F766E;");

    table_ = new QTableWidget(this);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);

    statusLabel_ = new QLabel(this);

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel("Report:", this));
    controls->addWidget(reportCombo_, 2);
    controls->addWidget(new QLabel("Period:", this));
    controls->addWidget(periodCombo_, 1);
    controls->addWidget(new QLabel("From:", this));
    controls->addWidget(fromDateEdit_);
    controls->addWidget(new QLabel("To:", this));
    controls->addWidget(toDateEdit_);
    controls->addWidget(refreshBtn_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(titleLabel_);
    layout->addWidget(table_, 1);
    layout->addWidget(statusLabel_);

    connect(reportCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ReportsPage::onReportChanged);
    connect(periodCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ReportsPage::onPeriodChanged);
    connect(fromDateEdit_, &QDateEdit::dateChanged, this, &ReportsPage::onDateChanged);
    connect(toDateEdit_, &QDateEdit::dateChanged, this, &ReportsPage::onDateChanged);
    connect(refreshBtn_, &QPushButton::clicked, this, &ReportsPage::refresh);

    syncDateControls();
    refresh();
}

void ReportsPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh();
}

void ReportsPage::onReportChanged(int) {
    syncDateControls();
    refresh();
}

void ReportsPage::onPeriodChanged(int) {
    syncDateControls();
    refresh();
}

void ReportsPage::onDateChanged() {
    if (periodCombo_->currentIndex() == 4) {
        refresh();
    }
}

void ReportsPage::syncDateControls() {
    const int idx = reportCombo_->currentIndex();
    const bool usesRange = (idx >= 0 && static_cast<std::size_t>(idx) < registry_.all().size())
                               ? registry_.all()[idx]->usesDateRange()
                               : false;

    periodCombo_->setEnabled(usesRange);

    const int periodIdx = periodCombo_->currentIndex();
    const bool isCustom = (periodIdx == 4);

    if (usesRange && !isCustom) {
        Period p = Period::Today;
        switch (periodIdx) {
            case 0: p = Period::Today; break;
            case 1: p = Period::Last7Days; break;
            case 2: p = Period::ThisMonth; break;
            case 3: p = Period::ThisYear; break;
            default: p = Period::Today; break;
        }
        const DateRange r = rangeFor(p, todayText());
        QSignalBlocker b1(fromDateEdit_);
        QSignalBlocker b2(toDateEdit_);
        const QDate fromD = QDate::fromString(QString::fromStdString(r.from), "yyyy-MM-dd");
        const QDate toD = QDate::fromString(QString::fromStdString(r.to), "yyyy-MM-dd");
        if (fromD.isValid()) fromDateEdit_->setDate(fromD);
        if (toD.isValid()) toDateEdit_->setDate(toD);
    }

    fromDateEdit_->setEnabled(usesRange && isCustom);
    toDateEdit_->setEnabled(usesRange && isCustom);
}

DateRange ReportsPage::currentDateRange() const {
    const int periodIdx = periodCombo_->currentIndex();
    if (periodIdx == 4) {
        return {
            fromDateEdit_->date().toString("yyyy-MM-dd").toStdString(),
            toDateEdit_->date().toString("yyyy-MM-dd").toStdString()
        };
    }
    Period p = Period::Today;
    switch (periodIdx) {
        case 0: p = Period::Today; break;
        case 1: p = Period::Last7Days; break;
        case 2: p = Period::ThisMonth; break;
        case 3: p = Period::ThisYear; break;
        default: p = Period::Today; break;
    }
    return rangeFor(p, todayText());
}

void ReportsPage::refresh() {
    const int idx = reportCombo_->currentIndex();
    if (idx < 0 || static_cast<std::size_t>(idx) >= registry_.all().size()) {
        table_->clear();
        table_->setRowCount(0);
        table_->setColumnCount(0);
        titleLabel_->setText("No reports registered");
        statusLabel_->clear();
        return;
    }

    const IReport& report = *registry_.all()[idx];
    const DateRange range = currentDateRange();
    const ReportData data{catalog_, orders_.orders(), bills_.all()};
    const ReportTable result = report.build(data, range);

    titleLabel_->setText(QString::fromStdString(result.title));

    const int cols = static_cast<int>(result.headers.size());
    table_->clear();
    table_->setColumnCount(cols);
    QStringList headerLabels;
    for (const std::string& h : result.headers) {
        headerLabels.append(QString::fromStdString(h));
    }
    table_->setHorizontalHeaderLabels(headerLabels);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    const bool hasTotals = !result.totalsRow.empty();
    const int dataRows = static_cast<int>(result.rows.size());
    table_->setRowCount(dataRows + (hasTotals ? 1 : 0));

    for (int r = 0; r < dataRows; ++r) {
        const auto& rowVec = result.rows[r];
        for (int c = 0; c < cols; ++c) {
            const QString val = (c < static_cast<int>(rowVec.size()))
                                    ? QString::fromStdString(rowVec[c])
                                    : QString();
            auto* item = new QTableWidgetItem(val);
            table_->setItem(r, c, item);
        }
    }

    if (hasTotals) {
        const int totalRowIdx = dataRows;
        for (int c = 0; c < cols; ++c) {
            const QString val = (c < static_cast<int>(result.totalsRow.size()))
                                    ? QString::fromStdString(result.totalsRow[c])
                                    : QString();
            auto* item = new QTableWidgetItem(val);
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            item->setBackground(QBrush(QColor("#E2E8F0")));
            table_->setItem(totalRowIdx, c, item);
        }
    }

    QString summary = QString("%1 row(s)").arg(dataRows);
    if (report.usesDateRange()) {
        summary += QString(" | Period: %1 to %2")
                       .arg(QString::fromStdString(range.from), QString::fromStdString(range.to));
    }
    statusLabel_->setText(summary);
}
