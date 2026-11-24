// OWNER: Member 1. Main Application Window (builds role-gated tabs via AccessPolicy).
#include "MainWindow.h"
#include "AccessPolicy.h"
#include "CameraPanel.h"
#include "OrderPage.h"
#include "RoleDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <vector>

namespace {

QString roleName(Role role) {
    switch (role) {
        case Role::Cashier: return "Cashier";
        case Role::Manager: return "Manager";
    }
    return "Cashier";
}

} // namespace

MainWindow::MainWindow(const DataPaths& paths,
                       const ManagerAuth& auth,
                       Role initialRole,
                       const OperationResult& startupNotice,
                       QWidget* parent)
    : QMainWindow(parent),
      paths_(paths),
      auth_(auth),
      role_(initialRole),
      bills_(paths_.billFile()) {
    setWindowTitle("Point of Sale (PoS V2)");
    resize(1240, 760);

    // 1. Ensure data/ folder exists and copy sample files if missing
    paths_.ensureDataFolder();

    // 2. Load Product Catalog
    catalog_.load(paths_.productFile());

    // 3. Load Order Store
    store_ = OrderStore(paths_.orderFile(), paths_.orderDetailFile());
    store_.load();

    // 4. Load Bill Store
    bills_.load();

    // 5. Initialize Core Managers
    orders_ = std::make_unique<OrderManager>(catalog_, &store_);
    checkout_ = std::make_unique<CheckoutService>(catalog_, store_, bills_, paths_.productFile());

    setupChrome();
    applyRole(role_);

    if (!startupNotice.message.empty()) {
        showStatus(QString::fromStdString(startupNotice.message), startupNotice.success);
    }
}

MainWindow::~MainWindow() {
    stopAllCameras();
}

void MainWindow::setupChrome() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 4);
    root->setSpacing(6);

    auto* headerRow = new QHBoxLayout;
    auto* appTitle = new QLabel("PoS V2", central);
    appTitle->setStyleSheet("font-size: 14pt; font-weight: bold; color: #0F766E;");

    roleBadge_ = new QLabel(central);
    roleBadge_->setStyleSheet(
        "background: #E0F2FE; color: #0369A1; font-weight: bold; padding: 4px 10px; border-radius: 4px;");

    switchRoleBtn_ = new QPushButton("Switch Role", central);
    switchRoleBtn_->setCursor(Qt::PointingHandCursor);
    switchRoleBtn_->setStyleSheet("padding: 5px 12px; font-weight: bold;");
    connect(switchRoleBtn_, &QPushButton::clicked, this, &MainWindow::onSwitchRole);

    headerRow->addWidget(appTitle);
    headerRow->addStretch();
    headerRow->addWidget(roleBadge_);
    headerRow->addWidget(switchRoleBtn_);

    tabs_ = new QTabWidget(central);
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    root->addLayout(headerRow);
    root->addWidget(tabs_, 1);
    setCentralWidget(central);

    statusBar()->showMessage("Ready");
}

QWidget* MainWindow::makePlaceholderTab(const QString& title, const QString& waitingFor) const {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);

    auto* heading = new QLabel(title, page);
    heading->setAlignment(Qt::AlignCenter);
    heading->setStyleSheet("font-size: 16pt; font-weight: bold; color: #1F2937;");

    auto* note = new QLabel(waitingFor, page);
    note->setAlignment(Qt::AlignCenter);
    note->setWordWrap(true);
    note->setStyleSheet("font-size: 11pt; color: #6B7280; margin-top: 6px;");

    layout->addWidget(heading);
    layout->addWidget(note);
    return page;
}

void MainWindow::applyRole(Role role) {
    stopAllCameras();
    role_ = role;
    roleBadge_->setText("Role: " + roleName(role_));

    QSignalBlocker blockTabs(tabs_);
    orderPage_ = nullptr;
    while (tabs_->count() > 0) {
        QWidget* w = tabs_->widget(0);
        tabs_->removeTab(0);
        delete w;
    }

    struct TabSpec {
        QString title;
        Action requiredAction;
        std::function<QWidget*()> createPage;
    };

    const std::vector<TabSpec> tabTable = {
        {"Orders", Action::RecordOrder, [this]() -> QWidget* {
            orderPage_ = new OrderPage(catalog_, *orders_, *checkout_, role_, auth_, tabs_);
            connect(orderPage_, &OrderPage::statusMessage, this, &MainWindow::showStatus);
            return orderPage_;
        }},
        {"Products", Action::ManageProducts, [this]() -> QWidget* {
            return makePlaceholderTab(
                "Products",
                "Placeholder tab: waiting for Member 2's ProductsPage (src/gui/ProductsPage.*).");
        }},
        {"Categories", Action::ManageCategories, [this]() -> QWidget* {
            return makePlaceholderTab(
                "Categories",
                "Placeholder tab: waiting for Member 2's CategoriesPage (src/gui/CategoriesPage.*).");
        }},
        {"Inventory", Action::ViewInventory, [this]() -> QWidget* {
            return makePlaceholderTab(
                "Inventory",
                "Placeholder tab: waiting for Member 3's InventoryPage (src/gui/InventoryPage.*).");
        }},
        {"Seasons", Action::ManageSeasons, [this]() -> QWidget* {
            return makePlaceholderTab(
                "Seasons",
                "Placeholder tab: waiting for Member 5's SeasonsPage (src/gui/SeasonsPage.*).");
        }},
        {"Reports", Action::ViewReports, [this]() -> QWidget* {
            return makePlaceholderTab(
                "Reports",
                "Placeholder tab: waiting for Member 5's ReportsPage (src/gui/ReportsPage.*).");
        }}
    };

    for (const auto& spec : tabTable) {
        if (AccessPolicy::isAllowed(role_, spec.requiredAction)) {
            tabs_->addTab(spec.createPage(), spec.title);
        }
    }

    syncCameraToCurrentTab();
    showStatus("Signed in as " + roleName(role_), true);
}

void MainWindow::stopAllCameras() {
    for (CameraPanel* cam : findChildren<CameraPanel*>()) {
        cam->stop();
    }
}

void MainWindow::syncCameraToCurrentTab() {
    QWidget* activePage = tabs_ ? tabs_->currentWidget() : nullptr;
    for (CameraPanel* cam : findChildren<CameraPanel*>()) {
        if (activePage && (activePage == cam || activePage->isAncestorOf(cam))) {
            continue;
        }
        cam->stop();
    }
    if (orderPage_ && activePage == orderPage_) {
        orderPage_->startCamera();
    } else if (activePage) {
        if (auto* cam = activePage->findChild<CameraPanel*>()) {
            cam->start();
        }
    }
}

void MainWindow::onTabChanged(int) {
    syncCameraToCurrentTab();
}

void MainWindow::onSwitchRole() {
    RoleDialog dlg(auth_, this);
    if (dlg.exec() == QDialog::Accepted) {
        applyRole(dlg.selectedRole());
    }
}

void MainWindow::showStatus(const QString& text, bool success) {
    statusBar()->setStyleSheet(success
        ? "QStatusBar { color: #1E4620; font-weight: bold; }"
        : "QStatusBar { color: #991B1B; font-weight: bold; }");
    statusBar()->showMessage(text);
}
