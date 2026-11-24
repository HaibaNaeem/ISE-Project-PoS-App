// OWNER: Member 1. Main Application Window.
#include "MainWindow.h"
#include "OrderPage.h"
#include "ProductsPage.h"
#include "CategoriesPage.h"
#include <QCoreApplication>
#include <QTabWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      paths_(QCoreApplication::applicationDirPath().toStdString()),
      bills_(paths_.billFile()) {
    setWindowTitle("Point of Sale (PoS V1)");
    resize(1200, 720);

    // 1. Ensure data/ folder exists and copy sample files if missing
    paths_.ensureDataFolder();

    // 2. Load Product Catalog and Categories
    catalog_.load(paths_.productFile());
    categories_.load(paths_.categoryFile());

    // 3. Load Order Store
    store_ = OrderStore(paths_.orderFile(), paths_.orderDetailFile());
    store_.load();

    // 4. Load Bill Store
    bills_.load();

    // 5. Initialize Core Managers with references
    orders_ = std::make_unique<OrderManager>(catalog_, &store_);
    checkout_ = std::make_unique<CheckoutService>(catalog_, store_, bills_, paths_.productFile());

    // 6. Setup tabs: Order, Products, Categories
    page_ = new OrderPage(catalog_, *orders_, *checkout_, this);

    auto* tabs = new QTabWidget;
    tabs->addTab(page_, "Order");
    tabs->addTab(new ProductsPage(catalog_, categories_, paths_.productFile(), this), "Products");
    tabs->addTab(new CategoriesPage(categories_, paths_.categoryFile(), this), "Categories");
    setCentralWidget(tabs);
}

MainWindow::~MainWindow() = default;