#pragma once
// OWNER: Member 5. Manager screen: read-only reports powered by ReportRegistry.
#include <QWidget>
#include "BillStore.h"
#include "OrderStore.h"
#include "ProductCatalog.h"
#include "ReportRegistry.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;

class ReportsPage : public QWidget {
    Q_OBJECT
public:
    ReportsPage(const ReportRegistry& registry,
                const ProductCatalog& catalog,
                const OrderStore& orders,
                const BillStore& bills,
                QWidget* parent = nullptr);

public slots:
    void refresh();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onReportChanged(int index);
    void onPeriodChanged(int index);
    void onDateChanged();

private:
    void syncDateControls();
    DateRange currentDateRange() const;

    const ReportRegistry& registry_;
    const ProductCatalog& catalog_;
    const OrderStore& orders_;
    const BillStore& bills_;

    QComboBox* reportCombo_ = nullptr;
    QComboBox* periodCombo_ = nullptr;
    QDateEdit* fromDateEdit_ = nullptr;
    QDateEdit* toDateEdit_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};
