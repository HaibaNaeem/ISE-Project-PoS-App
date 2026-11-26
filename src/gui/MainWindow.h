#pragma once
// OWNER: Member 1. Main Application Window (builds role-gated tabs via AccessPolicy).
#include <QMainWindow>
#include <memory>

#include "DataPaths.h"
#include "CategoryStore.h"
#include "ProductCatalog.h"
#include "OrderStore.h"
#include "BillStore.h"
#include "OrderManager.h"
#include "CheckoutService.h"
#include "InventoryService.h"
#include "SeasonStore.h"
#include "SeasonService.h"
#include "ReportRegistry.h"
#include "ManagerAuth.h"
#include "Role.h"

class QLabel;
class QPushButton;
class QTabWidget;
class OrderPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(const DataPaths& paths,
               const ManagerAuth& auth,
               Role initialRole,
               const OperationResult& startupNotice = {},
               QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onTabChanged(int index);
    void onSwitchRole();

private:
    void setupChrome();
    void applyRole(Role role);
    void stopAllCameras();
    void syncCameraToCurrentTab();
    void showStatus(const QString& text, bool success);
    QWidget* makePlaceholderTab(const QString& title, const QString& waitingFor) const;

    DataPaths paths_;
    ManagerAuth auth_;
    Role role_;

    CategoryStore categories_;
    ProductCatalog catalog_;
    OrderStore store_;
    BillStore bills_;
    SeasonStore seasonStore_;
    SeasonService seasonService_;
    ReportRegistry reportRegistry_;
    std::unique_ptr<OrderManager> orders_;
    std::unique_ptr<InventoryService> inventory_;
    std::unique_ptr<CheckoutService> checkout_;

    QLabel* roleBadge_ = nullptr;
    QPushButton* switchRoleBtn_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    OrderPage* orderPage_ = nullptr;
};