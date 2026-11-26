// OWNER: Member 4.
// Customer Bill Dialog implementation for Increment 3.
#include "BillDialog.h"
#include "BillPdfWriter.h"
#include "DialogStyle.h"
#include "Units.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QMessageBox>

namespace {

QString guiFormatMoney(Money amount) {
    return QString::fromStdString(formatMoney(amount) + " PKR");
}

QString guiFormatQuantity(Quantity qty) {
    if (qty % 1000 == 0) {
        return QString::number(qty / 1000);
    }
    return QString::fromStdString(formatQuantity(qty, MeasureUnit::Weight));
}

} // anonymous namespace

BillDialog::BillDialog(const Bill& bill, QWidget* parent)
    : QDialog(parent), bill_(bill) {
    setWindowTitle("Bill Summary - #" + QString::fromStdString(bill.id));
    setMinimumSize(780, 560);
    setStyleSheet(DialogStyle::dialogStyleSheet());

    setupUi();
}

void BillDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    // 1. Header Information Card (Bill ID, Order ID, Customer, Date/Time, Status Badge)
    auto* headerBox = new QGroupBox("Invoice Details", this);
    auto* headerLayout = new QGridLayout(headerBox);
    headerLayout->setContentsMargins(12, 12, 12, 12);
    headerLayout->setSpacing(8);

    auto* billIdLabel = new QLabel(QString("<b>Bill ID:</b> %1").arg(QString::fromStdString(bill_.id)), headerBox);
    auto* orderIdLabel = new QLabel(QString("<b>Order ID:</b> %1").arg(QString::fromStdString(bill_.orderId)), headerBox);
    auto* custLabel = new QLabel(QString("<b>Customer:</b> %1").arg(QString::fromStdString(bill_.customerName)), headerBox);

    QString dateTimeStr = QString::fromStdString(bill_.date + " " + bill_.time).trimmed();
    if (dateTimeStr.isEmpty()) dateTimeStr = "N/A";
    auto* dateLabel = new QLabel(QString("<b>Date/Time:</b> %1").arg(dateTimeStr), headerBox);

    QString statusStr = QString::fromStdString(orderStatusToText(bill_.status)).toUpper();
    auto* statusBadge = new QLabel(statusStr, headerBox);
    statusBadge->setAlignment(Qt::AlignCenter);
    QString badgeColor = (bill_.status == OrderStatus::Paid) ? DialogStyle::kSuccess : DialogStyle::kMutedText;
    statusBadge->setStyleSheet(QString(
        "background-color: %1; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 4px 12px;"
    ).arg(badgeColor));

    headerLayout->addWidget(billIdLabel, 0, 0);
    headerLayout->addWidget(orderIdLabel, 0, 1);
    headerLayout->addWidget(statusBadge, 0, 2, Qt::AlignRight);
    headerLayout->addWidget(custLabel, 1, 0);
    headerLayout->addWidget(dateLabel, 1, 1);

    mainLayout->addWidget(headerBox);

    // 2. Product Table (One row per product on the order, showing acronyms as per spec)
    auto* table = new QTableWidget(this);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        "Product Acronym", "Unit Price (PKR)", "Quantity", "Discount (PKR)"
    });

    table->setRowCount(static_cast<int>(bill_.lines.size()));
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);

    for (int i = 0; i < static_cast<int>(bill_.lines.size()); ++i) {
        const auto& line = bill_.lines[i];

        auto* itemAcronym = new QTableWidgetItem(QString::fromStdString(line.acronym));
        auto* itemPrice = new QTableWidgetItem(guiFormatMoney(line.unitPrice));
        auto* itemQty = new QTableWidgetItem(guiFormatQuantity(line.quantity));
        auto* itemDisc = new QTableWidgetItem(guiFormatMoney(line.discount));

        itemAcronym->setFont(QFont("Segoe UI", 10, QFont::Bold));
        itemPrice->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        itemQty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        itemDisc->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        table->setItem(i, 0, itemAcronym);
        table->setItem(i, 1, itemPrice);
        table->setItem(i, 2, itemQty);
        table->setItem(i, 3, itemDisc);
    }

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    mainLayout->addWidget(table);

    // 3. Totals Breakdown Card
    auto* summaryBox = new QGroupBox("Payment & Discount Summary", this);
    auto* summaryLayout = new QGridLayout(summaryBox);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    summaryLayout->setSpacing(6);

    auto addRow = [&](int row, const QString& label, const QString& val, bool bold = false, const QString& color = "") {
        auto* lbl = new QLabel(label, summaryBox);
        auto* v = new QLabel(val, summaryBox);
        v->setAlignment(Qt::AlignRight);
        if (bold) {
            QFont f = lbl->font(); f.setBold(true);
            lbl->setFont(f); v->setFont(f);
        }
        if (!color.isEmpty()) {
            lbl->setStyleSheet("color: " + color + ";");
            v->setStyleSheet("color: " + color + ";");
        }
        summaryLayout->addWidget(lbl, row, 0);
        summaryLayout->addWidget(v, row, 1);
    };

    int r = 0;
    addRow(r++, "Subtotal (without tax):", guiFormatMoney(bill_.subtotal));
    addRow(r++, "Tax:", "+ " + guiFormatMoney(bill_.tax));
    addRow(r++, "Item Discounts:", "- " + guiFormatMoney(bill_.itemDiscount));

    if (bill_.seasonalDiscount > 0) {
        addRow(r++, "Seasonal Discount:", "- " + guiFormatMoney(bill_.seasonalDiscount), true, "#16A34A");
    }
    if (bill_.specialDiscount > 0) {
        addRow(r++, "Special Discount:", "- " + guiFormatMoney(bill_.specialDiscount), true, "#2563EB");
    }

    addRow(r++, "Total Payable:", guiFormatMoney(bill_.payable), true, DialogStyle::kPrimary);
    addRow(r++, "Amount Received:", guiFormatMoney(bill_.received), true);
    addRow(r++, "Balance Returned:", guiFormatMoney(bill_.balance), true, DialogStyle::kSuccess);

    mainLayout->addWidget(summaryBox);

    // 4. PDF Status Label
    pdfStatusLabel_ = new QLabel(this);
    pdfStatusLabel_->setStyleSheet("color: #059669; font-weight: bold;");
    pdfStatusLabel_->setVisible(false);
    mainLayout->addWidget(pdfStatusLabel_);

    // 5. Actions (Print PDF, Close)
    auto* actionLayout = new QHBoxLayout();
    printPdfBtn_ = new QPushButton("Print PDF", this);
    printPdfBtn_->setStyleSheet("QPushButton { background-color: #0284C7; color: white; padding: 8px 16px; font-weight: bold; border-radius: 4px; }"
                                "QPushButton:hover { background-color: #0369A1; }");
    connect(printPdfBtn_, &QPushButton::clicked, this, &BillDialog::onPrintPdfClicked);

    closeBtn_ = new QPushButton("Close", this);
    connect(closeBtn_, &QPushButton::clicked, this, &QDialog::accept);

    actionLayout->addWidget(printPdfBtn_);
    actionLayout->addStretch();
    actionLayout->addWidget(closeBtn_);
    mainLayout->addLayout(actionLayout);
}

void BillDialog::onPrintPdfClicked() {
    OperationResult res = BillPdfWriter::writePdf(bill_);
    if (res.success) {
        pdfStatusLabel_->setText(QString("PDF Generated: %1").arg(QString::fromStdString(res.outputPath)));
        pdfStatusLabel_->setVisible(true);
        QMessageBox::information(this, "Bill PDF Generated",
                                 QString("Bill PDF successfully written to:\n%1").arg(QString::fromStdString(res.outputPath)));
    } else {
        QMessageBox::warning(this, "PDF Generation Failed", QString::fromStdString(res.message));
    }
}
