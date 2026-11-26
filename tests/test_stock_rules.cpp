#include "StockRules.h"
#include "TestHelpers.h"

int main() {
    // Stock 5: 0+5 ok, 0+6 no, 3+2 ok, 3+3 no, exact stock ok, requested 0 and negative handled
    CHECK(StockRules::hasEnoughStock(0, 5, 5));
    CHECK(!StockRules::hasEnoughStock(0, 6, 5));
    CHECK(StockRules::hasEnoughStock(3, 2, 5));
    CHECK(!StockRules::hasEnoughStock(3, 3, 5));
    CHECK(StockRules::hasEnoughStock(0, 5, 5));
    CHECK(StockRules::hasEnoughStock(0, 0, 5));
    CHECK(!StockRules::hasEnoughStock(0, -1, 5));
    CHECK(StockRules::hasEnoughStock(5, 0, 5));

    // Also verify global hasEnoughStock alias
    CHECK(hasEnoughStock(2, 3, 5));

    // Paid-order edit stock check: remaining stock 10 after selling 2 (available + origQty = 12)
    CHECK(StockRules::hasEnoughStock(0, 5000, 10000 + 2000));
    CHECK(StockRules::hasEnoughStock(0, 12000, 10000 + 2000));
    CHECK(!StockRules::hasEnoughStock(0, 13000, 10000 + 2000));

    return testSummary("test_stock_rules");
}
