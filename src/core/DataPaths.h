#pragma once
// OWNER: Member 5. No Qt here: main.cpp passes the .exe folder (QCoreApplication::applicationDirPath()).
#include <string>
#include "CommonTypes.h"

class DataPaths {
public:
    explicit DataPaths(std::string exeDir);

    // Creates <exeDir>/data/ if missing. For each of the four files that does not exist yet, copies it from
    // <exeDir>/sample_data/; if the sample is missing too, creates an empty file. Never overwrites existing data.
    OperationResult ensureDataFolder() const;

    std::string dataDir() const;
    std::string productFile() const;       // data/Product.txt
    std::string orderFile() const;         // data/Order.txt
    std::string orderDetailFile() const;   // data/OrderDetail.txt
    std::string billFile() const;          // data/Bill.txt
    std::string categoryFile() const;      // data/PCategory.txt
    // [WRITTEN BY MEMBER 4 on behalf of MEMBER 5 – minimal version so Member 4's billing work could build/test. Member 5: review, extend or replace; keep the agreed signatures.]
    std::string billDetailFile() const;    // data/BillDetail.txt

    // Product.txt stores image paths such as "sample_data/barcodes/123456.bmp" (relative to the .exe).
    std::string resolve(const std::string& relativePath) const;

private:
    std::string exeDir_;
};
