// Standalone tests for host/mods_used.cpp (FU Saves, item 11):
//   cl /std:c++20 /EHsc /I host tests/mods_used_tests.cpp host/mods_used.cpp
#include "mods_used.hpp"

#include <filesystem>
#include <iostream>

namespace mu = portablekit::fu::mods_used;
namespace fs = std::filesystem;

namespace {
int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << __FILE__ << ":" << __LINE__ << ": check failed: " #cond "\n"; \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

const mu::Mod kText{"luis-ptbr", "1.0.0", "Portugues (Brasil)", false};
const mu::Mod kSave{"more-pouch", "1.0", "Bigger pouch", true};
const mu::Mod kSave2{"more-pouch", "1.1", "Bigger pouch", true};
const mu::Mod kOther{"tweak", "2", "Tweak\tname", true};

void test_round_trip() {
    const auto parsed = mu::parse(mu::format({kText, kSave, kOther}));
    CHECK(parsed && parsed->size() == 3);
    CHECK((*parsed)[1].id == "more-pouch" && (*parsed)[1].affects_save && (*parsed)[1].version == "1.0");
    CHECK((*parsed)[2].name == "Tweak name");  // tabs in names cannot break the columns
    CHECK(mu::parse("FUMODSUSED 1\r\n") && mu::parse("FUMODSUSED 1\r\n")->empty());
    CHECK(!mu::parse("something else\n"));
    CHECK(!mu::parse("FUMODSUSED 1\nbad line\n"));
    CHECK(!mu::parse(""));
}

void test_compare() {
    CHECK(!mu::compare(std::vector<mu::Mod>{kText, kSave}, {kSave}).any());  // text mods do not count
    CHECK(!mu::compare(std::vector<mu::Mod>{kSave}, {kText, kSave}).any());
    auto d = mu::compare(std::vector<mu::Mod>{}, {kSave});
    CHECK(d.added.size() == 1 && d.added[0] == "Bigger pouch 1.0" && d.removed.empty());
    d = mu::compare(std::vector<mu::Mod>{kSave}, {});
    CHECK(d.removed.size() == 1 && d.added.empty());
    d = mu::compare(std::vector<mu::Mod>{kSave}, {kSave2});  // a new version counts
    CHECK(d.added.size() == 1 && d.removed.size() == 1);
    d = mu::compare(std::nullopt, {kText});
    CHECK(!d.any());
    d = mu::compare(std::nullopt, {kSave});
    CHECK(d.added.size() == 1);
}

void test_file() {
    const fs::path dir = fs::temp_directory_path() / "fusaves-mods-used-tests";
    fs::remove_all(dir);
    CHECK(mu::file_for(dir / "ms0") == dir / "mods-used.txt");
    CHECK(mu::file_for(dir / "ms0" / "") == dir / "mods-used.txt");
    const fs::path file = mu::file_for(dir / "ms0");
    CHECK(!mu::read(file));
    CHECK(mu::write(file, {kSave}));
    const auto back = mu::read(file);
    CHECK(back && back->size() == 1 && (*back)[0].id == "more-pouch");
    CHECK(!fs::exists(dir / "mods-used.txt.tmp"));
    fs::remove_all(dir);
}
} // namespace

int main() {
    test_round_trip();
    test_compare();
    test_file();
    if (failures == 0) std::cout << "mods_used_tests: all passed\n";
    return failures == 0 ? 0 : 1;
}
