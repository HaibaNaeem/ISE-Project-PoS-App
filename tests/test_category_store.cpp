#include <filesystem>
#include <fstream>
#include <iostream>
#include "CategoryStore.h"

static int failed = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "FAIL line " << __LINE__ << ": " #c "\n"; ++failed; } } while (0)
static std::string readAll(const std::string& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }

int main()
{
    CategoryStore s;
    CHECK(s.add({"12", "Dairy", "DAIRY"}).success);
    CHECK(s.add({"15", "Snacks", "SNACK"}).success);
    CHECK(!s.add({"5", "A", "A"}).success);
    CHECK(!s.add({"05", "B", "B"}).success);
    CHECK(!s.add({"9", "C", "C"}).success);
    CHECK(!s.add({"AB", "D", "D"}).success);
    CHECK(!s.add({"100", "E", "E"}).success);
    CHECK(!s.add({"12", "Other", "OTH"}).success);        // duplicate id
    CHECK(!s.add({"20", "Dairy", "OTH"}).success);        // duplicate name
    CHECK(!s.add({"21", "Long", "TOOLONGACR"}).success);  // acronym > 8
    CHECK(s.all().size() == 2);

    CHECK(s.update({"12", "Dairy Foods", "DAIRYF"}).success);
    CHECK(s.findById("12")->name == "Dairy Foods");
    CHECK(!s.update({"12", "Snacks", "X"}).success);      // name clash
    CHECK(!s.update({"99", "Nope", "N"}).success);

    CHECK(s.save("cs_test.txt").success);
    CategoryStore t;
    CHECK(t.load("cs_test.txt").success && t.all().size() == 2 && t.warnings().empty());
    CHECK(readAll("cs_test.txt") == t.serialize());

    { std::ofstream("cs_bad.txt") << "12 \"Dairy\" DAIRY #\nXX \"Bad\" B #\n12 \"Dup\" D #\n13 \"NoHash\" N\n"; }
    CategoryStore b;
    CHECK(b.load("cs_bad.txt").success && b.all().size() == 1 && b.warnings().size() == 3);
    CHECK(!b.load("cs_missing.txt").success);

    std::filesystem::remove("cs_test.txt");
    std::filesystem::remove("cs_bad.txt");
    std::cout << (failed ? "FAILED\n" : "All category store tests passed\n");
    return failed ? 1 : 0;
}