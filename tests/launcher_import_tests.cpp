// Standalone tests for the launcher's pre-boot import (FU Saves, item 11):
//   cl /std:c++20 /EHsc /I portablekit/host tests/launcher_import_tests.cpp
//      portablekit/host/save_data/launcher_import.cpp portablekit/host/save_data/auto_backup.cpp
//      portablekit/host/save_data/param_sfo.cpp
// Uses made-up files in a temporary folder; no real save, no key.
#include "save_data/launcher_import.hpp"
#include "save_data/param_sfo.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
namespace ab = portablekit::savedata::auto_backup;
namespace li = portablekit::savedata::launcher_import;
using portablekit::savedata::ParamSfo;

namespace {

int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << __FILE__ << ":" << __LINE__ << ": check failed: " #cond "\n"; \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

void write(const fs::path &file, const std::string &text) {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary | std::ios::trunc) << text;
}
std::string read(const fs::path &file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
void write_sfo(const fs::path &folder, const std::string &directory) {
    ParamSfo sfo;
    sfo.set_string("SAVEDATA_DIRECTORY", directory, 64);
    sfo.set_string("TITLE", "MONSTER HUNTER FREEDOM UNITE", 128);
    const auto bytes = sfo.serialize();
    fs::create_directories(folder);
    std::ofstream(folder / "PARAM.SFO", std::ios::binary)
        .write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
void make_save(const fs::path &folder, const std::string &bin) {
    write_sfo(folder, "ULUS10391");
    write(folder / "MHP2NDG.BIN", bin);
}

struct Sandbox {
    fs::path root;
    ab::Config config;
    explicit Sandbox(const char *name) {
        root = fs::temp_directory_path() / ("fusaves-import-tests-" + std::string(name));
        fs::remove_all(root);
        config = ab::make_config(root / "data" / "ms0", "ULUS10391", "save-preview/ULUS10391.txt", 5);
    }
    ~Sandbox() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    // Like import_save(): copy beside, move the old one to .backup, move in.
    li::Importer importer() const {
        return [this](const fs::path &from) {
            li::Imported out;
            const fs::path dest = config.save_folder;
            const fs::path staging = dest.parent_path() / ".import-ULUS10391";
            fs::remove_all(staging);
            fs::create_directories(staging);
            fs::copy(from, staging, fs::copy_options::recursive);
            if (fs::exists(dest)) {
                out.replaced = dest.parent_path() / ".backup" / "t" / "ULUS10391";
                fs::create_directories(out.replaced.parent_path());
                fs::rename(dest, out.replaced);
            }
            fs::rename(staging, dest);
            out.ok = true;
            return out;
        };
    }
};

void test_check() {
    Sandbox box("check");
    const fs::path stick = box.root / "psp";
    const fs::path save = stick / "PSP" / "SAVEDATA" / "ULUS10391";
    CHECK(li::check_folder(save, "ULUS10391", "MHP2NDG.BIN").problem == "saves.import_pre.problem.no_save");
    make_save(save, "data");
    CHECK(li::check_folder(save, "ULUS10391", "MHP2NDG.BIN").ok());
    // The SAVEDATA folder, or the stick's root, finds the save folder in it.
    CHECK(li::check_folder(stick / "PSP" / "SAVEDATA", "ULUS10391", "MHP2NDG.BIN").folder == save);
    CHECK(li::check_folder(stick, "ULUS10391", "MHP2NDG.BIN").folder == save);
    write(save / "MHP2NDG.BIN", "");
    CHECK(li::check_folder(save, "ULUS10391", "MHP2NDG.BIN").problem == "saves.import_pre.problem.no_data");
    write(save / "PARAM.SFO", "not a param.sfo");
    CHECK(li::check_folder(save, "ULUS10391", "MHP2NDG.BIN").problem == "saves.import_pre.problem.bad_sfo");
    const fs::path other = box.root / "other" / "ULUS10391DAT";
    write_sfo(other, "ULUS10391DAT");
    write(other / "MHP2NDG.BIN", "x");
    CHECK(li::check_folder(other, "ULUS10391", "MHP2NDG.BIN").problem == "saves.import_pre.problem.other_save");
}

void test_import_backs_up_first_and_keeps_dot_backup() {
    Sandbox box("import");
    make_save(box.config.save_folder, "current");
    write(box.config.summary_file, "summary");
    const fs::path incoming = box.root / "psp" / "ULUS10391";
    make_save(incoming, "from psp");
    const li::Check check = li::check_folder(incoming, "ULUS10391", "MHP2NDG.BIN");
    const li::Result r = li::import_with_backup(box.config, check, std::chrono::system_clock::now(), box.importer());
    CHECK(r.ok);
    CHECK(r.backup.ok && !r.backup.skipped);
    CHECK(read(r.backup.folder / "ULUS10391" / "MHP2NDG.BIN") == "current");
    CHECK(read(box.config.save_folder / "MHP2NDG.BIN") == "from psp");
    CHECK(read(r.replaced / "MHP2NDG.BIN") == "current");
}

void test_import_without_current_save() {
    Sandbox box("import-empty");
    const fs::path incoming = box.root / "psp" / "ULUS10391";
    make_save(incoming, "from psp");
    const li::Result r = li::import_with_backup(box.config, li::check_folder(incoming, "ULUS10391", "MHP2NDG.BIN"),
                                                std::chrono::system_clock::now(), box.importer());
    CHECK(r.ok && r.backup.skipped && r.replaced.empty());
    CHECK(read(box.config.save_folder / "MHP2NDG.BIN") == "from psp");
}

void test_refused_check_copies_nothing() {
    Sandbox box("import-refused");
    make_save(box.config.save_folder, "current");
    bool called = false;
    const li::Result r = li::import_with_backup(
        box.config, li::check_folder(box.root / "nothing", "ULUS10391", "MHP2NDG.BIN"),
        std::chrono::system_clock::now(), [&](const fs::path &) {
            called = true;
            return li::Imported{};
        });
    CHECK(!r.ok && !called);
    CHECK(read(box.config.save_folder / "MHP2NDG.BIN") == "current");
}

} // namespace

int main() {
    test_check();
    test_import_backs_up_first_and_keeps_dot_backup();
    test_import_without_current_save();
    test_refused_check_copies_nothing();
    if (failures == 0) std::cout << "launcher_import_tests: all passed\n";
    return failures == 0 ? 0 : 1;
}
