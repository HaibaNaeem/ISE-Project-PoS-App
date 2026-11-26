#pragma once
// OWNER: Member 5. Manager screen: view, add, and activate/deactivate seasonal discounts.
#include <QWidget>
#include <string>
#include "SeasonStore.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;

class SeasonsPage : public QWidget {
    Q_OBJECT
public:
    SeasonsPage(SeasonStore& store, std::string seasonsPath, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void seasonsChanged();

private slots:
    void onAdd();
    void onItemChanged(QTableWidgetItem* item);

private:
    void showStatus(const QString& text, bool ok);

    SeasonStore& store_;
    std::string seasonsPath_;

    QTableWidget* table_ = nullptr;
    QLineEdit* startDateEdit_ = nullptr;
    QLineEdit* endDateEdit_ = nullptr;
    QLineEdit* percentEdit_ = nullptr;
    QCheckBox* activeCheck_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};
