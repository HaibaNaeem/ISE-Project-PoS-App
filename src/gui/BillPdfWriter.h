#pragma once
// OWNER: Member 4.
// Generates PDF invoices using Qt's QPdfWriter and QTextDocument.
#include <string>
#include "Bill.h"
#include "CommonTypes.h"

class BillPdfWriter {
public:
    // Generates a PDF for the given bill.
    // If outputPath is empty, defaults to "data/bills/Bill_<id>.pdf".
    // Returns OperationResult where message and outputPath contain the generated PDF path.
    static OperationResult writePdf(const Bill& bill, const std::string& outputPath = "");
};
