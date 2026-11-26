
#include "ProductEditorDialog.h"
#include "BarcodeAllocator.h"
#include "DialogStyle.h"
#include "Units.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString qs(const std::string& s) {
    return QString::fromStdString(s);
}
}

ProductEditorDialog::ProductEditorDialog(
    ProductCatalog& catalog,
    const CategoryStore& categories,
    const std::string& productPath,
    const Product* editing,
    QWidget* parent)
    : QDialog(parent),
      catalog_(catalog),
      path_(productPath),
      editing_(editing != nullptr)
{
    if (editing)
        original_ = *editing;

    setWindowTitle(editing_ ? "Edit product" : "Add product");
    setStyleSheet(DialogStyle::dialogStyleSheet());
    setMinimumWidth(460);

    category_ = new QComboBox;

    for (const auto& c : categories.all())
        category_->addItem(qs(c.id) + " - " + qs(c.name), qs(c.id));

    unit_ = new QComboBox;
    unit_->addItem("count");
    unit_->addItem("weight");

    name_ = new QLineEdit;
    acronym_ = new QLineEdit;
    acronym_->setMaxLength(8);

    price_ = new QLineEdit;
    price_->setPlaceholderText("e.g. 450.00");

    tax_ = new QLineEdit;
    tax_->setPlaceholderText("per unit, e.g. 20.00");

    discount_ = new QLineEdit;
    discount_->setPlaceholderText("per unit, e.g. 10.00");

    stock_ = new QLineEdit;
    stock_->setPlaceholderText("count: 12   weight: 25.500");

    barcode_ = new QLabel("-");
    barcode_->setStyleSheet("font-weight: bold;");

    image_ = new QLabel;
    image_->setAlignment(Qt::AlignCenter);
    image_->setMinimumHeight(110);

    message_ = new QLabel;
    message_->setWordWrap(true);

    auto* form = new QFormLayout;
    form->addRow("Category:", category_);
    form->addRow("Barcode:", barcode_);
    form->addRow("Name:", name_);
    form->addRow("Acronym:", acronym_);
    form->addRow("Unit:", unit_);
    form->addRow("Unit price (PKR):", price_);
    form->addRow("Tax per unit (PKR):", tax_);
    form->addRow("Discount per unit (PKR):", discount_);
    form->addRow("In stock:", stock_);

    saveBtn_ = new QPushButton("Save");
    saveBtn_->setObjectName("confirmBtn");

    auto* cancelBtn = new QPushButton("Cancel");
    cancelBtn->setObjectName("cancelBtn");

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(saveBtn_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(image_);
    layout->addWidget(message_);
    layout->addLayout(buttons);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn_, &QPushButton::clicked, this, &ProductEditorDialog::onSave);

    connect(
        category_,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &ProductEditorDialog::onCategoryChanged
    );

    if (editing_) {
        category_->setCurrentIndex(category_->findData(qs(original_.categoryId)));
        category_->setEnabled(false);

        unit_->setCurrentText(qs(measureUnitToText(original_.unit)));
        unit_->setEnabled(false);

        barcode_->setText(qs(original_.barcode));
        name_->setText(qs(original_.name));
        acronym_->setText(qs(original_.acronym));
        price_->setText(qs(formatMoney(original_.unitPrice)));
        tax_->setText(qs(formatMoney(original_.taxPerUnit)));
        discount_->setText(qs(formatMoney(original_.discountPerUnit)));
        stock_->setText(qs(formatQuantity(original_.available, original_.unit)));

        showImage(qs(original_.barcodeImagePath));
    }
    else if (category_->count() == 0) {
        showError("Add a category first (Categories tab).");
        saveBtn_->setEnabled(false);
    }
    else {
        onCategoryChanged();
    }
}

void ProductEditorDialog::showError(const QString& text)
{
    message_->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(DialogStyle::kDanger)
    );
    message_->setText(text);
}

void ProductEditorDialog::showImage(const QString& relativePath)
{
    if (relativePath.isEmpty()) {
        image_->setText("image not found");
        return;
    }

    QString path = relativePath;

    if (QDir::isRelativePath(path))
        path = QCoreApplication::applicationDirPath() + "/" + path;

    QPixmap pix(path);

    if (pix.isNull()) {
        image_->setText("image not found");
        return;
    }

    image_->setPixmap(pix.scaledToWidth(300, Qt::FastTransformation));
}

void ProductEditorDialog::onCategoryChanged()
{
    if (editing_)
        return;

    message_->clear();

    BarcodeAllocator alloc;

    OperationResult r = alloc.nextBarcode(
        category_->currentData().toString().toStdString(),
        catalog_
    );

    if (!r.success) {
        barcode_->setText("-");
        showError(qs(r.message));
        saveBtn_->setEnabled(false);
        return;
    }

    saveBtn_->setEnabled(true);
    barcode_->setText(qs(r.value));
    image_->setPixmap(QPixmap());
    image_->setText("Barcode image is created when you save.");
}

void ProductEditorDialog::onSave()
{
    message_->clear();

    Product p = editing_ ? original_ : Product{};

    // FIX: Assign the entered product name to the Product object.
    p.name = name_->text().trimmed().replace(' ', '_').toStdString();

    p.acronym = acronym_->text().trimmed().toStdString();

    if (!editing_) {
        OperationResult u = parseMeasureUnit(
            unit_->currentText().toStdString(),
            p.unit
        );

        if (!u.success) {
            showError(qs(u.message));
            return;
        }
    }

    OperationResult r = parseMoney(
        price_->text().trimmed().toStdString(),
        p.unitPrice
    );

    if (!r.success) {
        showError("Unit price: " + qs(r.message));
        return;
    }

    r = parseMoney(
        tax_->text().trimmed().toStdString(),
        p.taxPerUnit
    );

    if (!r.success) {
        showError("Tax: " + qs(r.message));
        return;
    }

    r = parseMoney(
        discount_->text().trimmed().toStdString(),
        p.discountPerUnit
    );

    if (!r.success) {
        showError("Discount: " + qs(r.message));
        return;
    }

    r = parseQuantity(
        stock_->text().trimmed().toStdString(),
        p.unit,
        p.available
    );

    if (!r.success) {
        showError("Stock: " + qs(r.message));
        return;
    }

    if (editing_) {
        r = catalog_.update(p);

        if (!r.success) {
            showError(qs(r.message));
            return;
        }
    }
    else {
        BarcodeAllocator alloc;

        OperationResult b = alloc.nextBarcode(
            category_->currentData().toString().toStdString(),
            catalog_
        );

        if (!b.success) {
            showError(qs(b.message));
            return;
        }

        p.barcode = b.value;
        p.categoryId = p.barcode.substr(0, 2);
        p.barcodeImagePath = "sample_data/barcodes/" + p.barcode + ".bmp";

        // Image is relative to the folder of the .exe
        const QString abs =
            QCoreApplication::applicationDirPath() + "/" + qs(p.barcodeImagePath);

        QDir().mkpath(QFileInfo(abs).absolutePath());

        OperationResult img = alloc.makeImage(p.barcode, abs.toStdString());

        if (!img.success) {
            showError("Could not create barcode image: " + qs(img.message));
            return;
        }

        r = catalog_.add(p);

        if (!r.success) {
            QFile::remove(abs);
            showError(qs(r.message));
            return;
        }

        showImage(qs(p.barcodeImagePath));
    }

    OperationResult s = catalog_.save(path_);

    if (!s.success) {
        showError(
            "Saved in memory, but the file could not be written: " + qs(s.message)
        );
        return;
    }

    accept();
}
