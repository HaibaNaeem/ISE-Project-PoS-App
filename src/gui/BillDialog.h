#pragma once
// OWNER: Member 4.
// Customer Bill Dialog for Increment 3:
// Displays detailed product lines with acronyms, unit prices, quantities, discounts,
// full financial breakdown including seasonal and special discounts, amount received, and balance,
// along with a "Print PDF" button that generates a PDF bill and shows the path.
#include <QDialog>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include "Bill.h"

class BillDialog : public QDialog {
    Q_OBJECT

public:
    explicit BillDialog(const Bill& bill, QWidget* parent = nullptr);

private slots:
    void onPrintPdfClicked();

private:
    void setupUi();

    Bill bill_;
    QLabel* pdfStatusLabel_ = nullptr;
    QPushButton* printPdfBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;
};
