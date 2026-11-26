#pragma once
// OWNER: Member 2. Manager screen: list, add and rename categories.
#include <QWidget>
#include <string>
#include "CategoryStore.h"

class QLabel;
class QLineEdit;
class QTableWidget;

class CategoriesPage : public QWidget {
    Q_OBJECT
public:
    CategoriesPage(CategoryStore& store, const std::string& path, QWidget* parent = nullptr);
    void refresh();

signals:
    void categoriesChanged();

private slots:
    void onAdd();
    void onRename();
    void onRowSelected();

private:
    void showMessage(const QString& text, bool ok);

    CategoryStore& store_;
    std::string path_;
    QTableWidget* table_ = nullptr;
    QLineEdit* id_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* acronym_ = nullptr;
    QLabel* status_ = nullptr;
};