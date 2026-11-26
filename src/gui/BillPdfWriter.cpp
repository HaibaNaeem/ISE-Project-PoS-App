// OWNER: Member 4.
// Bill PDF generator implementation using QPdfWriter and QTextDocument.
#include "BillPdfWriter.h"
#include "Units.h"
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QDir>
#include <QFileInfo>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

QString formatPaisa(Money amount) {
    return QString::fromStdString(formatMoney(amount) + " PKR");
}

QString formatQtyPdf(Quantity qty) {
    if (qty % 1000 == 0) {
        return QString::number(qty / 1000);
    }
    return QString::fromStdString(formatQuantity(qty, MeasureUnit::Weight));
}

} // anonymous namespace

OperationResult BillPdfWriter::writePdf(const Bill& bill, const std::string& outputPath) {
    std::string targetPath = outputPath;
    if (targetPath.empty()) {
        std::string billsDir = "data/bills";
        std::error_code ec;
        fs::create_directories(billsDir, ec);
        targetPath = billsDir + "/Bill_" + bill.id + ".pdf";
    } else {
        std::error_code ec;
        fs::path p(targetPath);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path(), ec);
        }
    }

    QString html;
    html += "<html><head><style>";
    html += "body { font-family: Arial, sans-serif; font-size: 11pt; color: #333333; margin: 20px; }";
    html += "h1 { color: #1E293B; font-size: 20pt; margin-bottom: 4px; text-align: center; }";
    html += "p.sub { text-align: center; color: #64748B; font-size: 10pt; margin-top: 0; }";
    html += "table.info { width: 100%; margin-bottom: 20px; border-collapse: collapse; }";
    html += "table.info td { padding: 4px 8px; font-size: 10pt; }";
    html += "table.lines { width: 100%; border-collapse: collapse; margin-top: 15px; margin-bottom: 20px; }";
    html += "table.lines th { background-color: #F1F5F9; border-bottom: 2px solid #CBD5E1; padding: 8px; text-align: left; font-size: 10pt; }";
    html += "table.lines td { border-bottom: 1px solid #E2E8F0; padding: 8px; font-size: 10pt; }";
    html += "table.totals { width: 100%; margin-top: 10px; border-collapse: collapse; }";
    html += "table.totals td { padding: 4px 8px; font-size: 10pt; }";
    html += ".right { text-align: right; }";
    html += ".bold { font-weight: bold; }";
    html += ".payable { font-size: 14pt; color: #0284C7; font-weight: bold; }";
    html += "</style></head><body>";

    // Header
    html += "<h1>POINT OF SALE - INVOICE</h1>";
    html += "<p class=\"sub\">Official Customer Receipt</p>";
    html += "<hr style=\"border: 0; border-top: 1px solid #CBD5E1;\">";

    // Meta details
    html += "<table class=\"info\">";
    html += QString("<tr><td><b>Bill ID:</b> %1</td><td class=\"right\"><b>Date:</b> %2</td></tr>")
                .arg(QString::fromStdString(bill.id), QString::fromStdString(bill.date));
    html += QString("<tr><td><b>Order ID:</b> %1</td><td class=\"right\"><b>Time:</b> %2</td></tr>")
                .arg(QString::fromStdString(bill.orderId), QString::fromStdString(bill.time));
    html += QString("<tr><td><b>Customer:</b> %1</td><td class=\"right\"><b>Status:</b> %2</td></tr>")
                .arg(QString::fromStdString(bill.customerName),
                     QString::fromStdString(orderStatusToText(bill.status)).toUpper());
    html += "</table>";

    // Products table (showing acronyms as per spec)
    html += "<table class=\"lines\">";
    html += "<tr>";
    html += "<th>Item Acronym</th>";
    html += "<th class=\"right\">Unit Price</th>";
    html += "<th class=\"right\">Qty</th>";
    html += "<th class=\"right\">Discount</th>";
    html += "</tr>";

    for (const auto& line : bill.lines) {
        html += "<tr>";
        html += QString("<td><b>%1</b></td>").arg(QString::fromStdString(line.acronym));
        html += QString("<td class=\"right\">%1</td>").arg(formatPaisa(line.unitPrice));
        html += QString("<td class=\"right\">%1</td>").arg(formatQtyPdf(line.quantity));
        html += QString("<td class=\"right\">%1</td>").arg(formatPaisa(line.discount));
        html += "</tr>";
    }
    html += "</table>";

    // Totals Breakdown
    html += "<table class=\"totals\">";
    html += QString("<tr><td class=\"bold\">Subtotal (without tax):</td><td class=\"right\">%1</td></tr>")
                .arg(formatPaisa(bill.subtotal));
    html += QString("<tr><td class=\"bold\">Tax:</td><td class=\"right\">+ %1</td></tr>")
                .arg(formatPaisa(bill.tax));
    html += QString("<tr><td class=\"bold\">Product Discounts:</td><td class=\"right\">- %1</td></tr>")
                .arg(formatPaisa(bill.itemDiscount));

    if (bill.seasonalDiscount > 0) {
        html += QString("<tr><td class=\"bold\" style=\"color: #16A34A;\">Seasonal Discount:</td><td class=\"right\" style=\"color: #16A34A;\">- %1</td></tr>")
                    .arg(formatPaisa(bill.seasonalDiscount));
    }
    if (bill.specialDiscount > 0) {
        html += QString("<tr><td class=\"bold\" style=\"color: #2563EB;\">Special Discount:</td><td class=\"right\" style=\"color: #2563EB;\">- %1</td></tr>")
                    .arg(formatPaisa(bill.specialDiscount));
    }

    html += "<tr><td colspan=\"2\"><hr style=\"border: 0; border-top: 1px solid #CBD5E1;\"></td></tr>";
    html += QString("<tr><td class=\"payable\">TOTAL PAYABLE:</td><td class=\"right payable\">%1</td></tr>")
                .arg(formatPaisa(bill.payable));
    html += QString("<tr><td class=\"bold\">Amount Received:</td><td class=\"right bold\">%1</td></tr>")
                .arg(formatPaisa(bill.received));
    html += QString("<tr><td class=\"bold\">Balance Returned:</td><td class=\"right bold\">%1</td></tr>")
                .arg(formatPaisa(bill.balance));
    html += "</table>";

    html += "<br><hr style=\"border: 0; border-top: 1px solid #CBD5E1;\">";
    html += "<p class=\"sub\">Thank you for shopping with us!</p>";
    html += "</body></html>";

    QTextDocument doc;
    doc.setHtml(html);

    QPdfWriter writer(QString::fromStdString(targetPath));
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(96);
    doc.print(&writer);

    return {true, "PDF generated successfully at: " + targetPath, targetPath, targetPath};
}
