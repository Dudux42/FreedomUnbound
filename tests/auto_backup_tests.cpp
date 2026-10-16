// Standalone tests for portablekit/host/save_data/auto_backup.cpp (FU Saves):
// rotation, change detection and restore. Plain asserts, no framework:
//   cl /std:c++20 /EHsc /I portablekit/host tests/auto_backup_tests.cpp portablekit/host/save_data/auto_backup.cpp
//   g++ -std=c++20 -I portablekit/host tests/auto_backup_tests.cpp portablekit/host/save_data/auto_backup.cpp
// Works in a fresh temporary folder; never touches real saves.
#include <random>
#include "save_data/auto_backup.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
namespace ab = portablekit::savedata::auto_backup;
using clock_type = std::chrono::system_clock;

namespace {

int failures = 0;

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            std::cerr << __FILE__ << ":" << __LINE__ << ": check failed: " #cond "\n";      \
            ++failures;                                                                    \
        }                                                                                  \
    } while (0)

void write(const fs::path &file, const std::string &text) {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary | std::ios::trunc) << text;
}

std::string read(const fs::path &file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

struct Sandbox {
    fs::path root;
    ab::Config config;
    explicit Sandbox(const char *name) {
        root = fs::temp_directory_path() / ("fusaves-tests-" + std::to_string(std::random_device{}()) + "-" + std::string(name));
        fs::remove_all(root);
        // <data>/ms0/PSP/SAVEDATA/ULUS10391, with an accented data folder name.
        const fs::path stick = root / fs::path(u8"Dados do Usu\u00e1rio") / "ms0";
        config = ab::make_config(stick, "ULUS10391", "save-preview/ULUS10391.txt", 5);
    }
    ~Sandbox() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    void save(const std::string &bin, const std::string &summary) {
        write(config.save_folder / "MHP2NDG.BIN", bin);
        write(config.save_folder / "PARAM.SFO", "sfo");
        write(config.summary_file, summary);
    }
};

clock_type::time_point at(int seconds) {
    // A fixed local day, a second apart per step.
    return clock_type::from_time_t(1'760'400'000) + std::chrono::seconds(seconds);
}

void test_stamps() {
    CHECK(ab::is_stamp_name("20261014-001705"));
    CHECK(!ab::is_stamp_name("20261014-001705.partial"));
    CHECK(!ab::is_stamp_name("2026101-0017055"));
    CHECK(!ab::is_stamp_name("notes"));
    CHECK(ab::stamp_for_display("20261014-001705") == "2026-10-14 00:17:05");
    CHECK(ab::is_stamp_name(ab::stamp_name(clock_type::now())));
}

void test_config() {
    const ab::Config c = ab::make_config(fs::path("data") / "ms0" / "", "ULUS10391", "save-preview/ULUS10391.txt", 5);
    CHECK(c.save_folder == fs::path("data") / "ms0" / "PSP" / "SAVEDATA" / "ULUS10391");
    CHECK(c.root == fs::path("data") / "SaveBackups");
    CHECK(c.summary_file == fs::path("data") / "save-preview" / "ULUS10391.txt");
}

void test_no_save_is_skipped() {
    Sandbox box("nosave");
    const ab::Result r = ab::back_up_if_changed(box.config, at(0));
    CHECK(r.ok && r.skipped);
    CHECK(ab::list(box.config).empty());
}

void test_backup_only_when_changed() {
    Sandbox box("changed");
    box.save("v1", "summary v1");
    ab::Result r = ab::back_up_if_changed(box.config, at(0));
    CHECK(r.ok && !r.skipped);
    CHECK(read(r.folder / "ULUS10391" / "MHP2NDG.BIN") == "v1");
    CHECK(read(r.folder / "ULUS10391.txt") == "summary v1");
    r = ab::back_up_if_changed(box.config, at(1));
    CHECK(r.ok && r.skipped);
    CHECK(ab::list(box.config).size() == 1);
    box.save("v2", "summary v2");
    r = ab::back_up_if_changed(box.config, at(2));
    CHECK(r.ok && !r.skipped);
    CHECK(ab::list(box.config).size() == 2);
    // An added file counts as a change too.
    write(box.config.save_folder / "extra.bin", "x");
    CHECK(ab::changed_since_last_backup(box.config));
}

void test_rotation_keeps_newest_five() {
    Sandbox box("rotate");
    write(box.config.root / "notes.txt", "mine");          // not a backup: left alone
    fs::create_directories(box.config.root / "my-folder");  // likewise
    for (int i = 0; i < 8; ++i) {
        box.save("v" + std::to_string(i), "s" + std::to_string(i));
        CHECK(ab::back_up_if_changed(box.config, at(i * 10)).ok);
    }
    const auto backups = ab::list(box.config);
    CHECK(backups.size() == 5);
    CHECK(read(backups.front().save(box.config) / "MHP2NDG.BIN") == "v7");
    CHECK(read(backups.back().save(box.config) / "MHP2NDG.BIN") == "v3");
    CHECK(fs::exists(box.config.root / "notes.txt"));
    CHECK(fs::exists(box.config.root / "my-folder"));
}

void test_same_second_gets_next_name() {
    Sandbox box("samesecond");
    box.save("a", "s");
    const ab::Result first = ab::back_up(box.config, at(0));
    const ab::Result second = ab::back_up(box.config, at(0));
    CHECK(first.ok && second.ok && first.folder != second.folder);
}

void test_partial_is_cleaned() {
    Sandbox box("partial");
    box.save("a", "s");
    fs::create_directories(box.config.root / "20200101-000000.partial" / "ULUS10391");
    CHECK(ab::back_up(box.config, at(0)).ok);
    CHECK(!fs::exists(box.config.root / "20200101-000000.partial"));
    CHECK(ab::list(box.config).size() == 1);
}

void test_restore_backs_up_current_first() {
    Sandbox box("restore");
    box.save("old", "summary old");
    CHECK(ab::back_up_if_changed(box.config, at(0)).ok);
    box.save("new", "summary new");  // played since, not yet backed up
    const ab::Entry chosen = ab::list(box.config).front();
    const ab::Result r = ab::restore(box.config, chosen, at(5));
    CHECK(r.ok);
    CHECK(read(box.config.save_folder / "MHP2NDG.BIN") == "old");
    CHECK(read(box.config.summary_file) == "summary old");
    const auto backups = ab::list(box.config);
    CHECK(backups.size() == 2);
    CHECK(read(backups.front().save(box.config) / "MHP2NDG.BIN") == "new");
    CHECK(read(backups.front().summary(box.config)) == "summary new");
    CHECK(!fs::exists(fs::path(box.config.save_folder.native() + fs::path(".restoring").native())));
    CHECK(!fs::exists(fs::path(box.config.save_folder.native() + fs::path(".replaced").native())));
    // The restored file keeps the backup's time, which the summary's stamp names.
    CHECK(fs::last_write_time(box.config.save_folder / "MHP2NDG.BIN") ==
          fs::last_write_time(chosen.save(box.config) / "MHP2NDG.BIN"));
}

void test_restore_unchanged_makes_no_extra_backup() {
    Sandbox box("restore-same");
    box.save("a", "s");
    CHECK(ab::back_up_if_changed(box.config, at(0)).ok);
    CHECK(ab::restore(box.config, ab::list(box.config).front(), at(1)).ok);
    CHECK(ab::list(box.config).size() == 1);
}

void test_restore_oldest_survives_rotation() {
    Sandbox box("restore-oldest");
    for (int i = 0; i < 5; ++i) {
        box.save("v" + std::to_string(i), "s");
        CHECK(ab::back_up_if_changed(box.config, at(i * 10)).ok);
    }
    box.save("current", "s");
    const ab::Entry oldest = ab::list(box.config).back();
    const ab::Result r = ab::restore(box.config, oldest, at(100));
    CHECK(r.ok);
    CHECK(read(box.config.save_folder / "MHP2NDG.BIN") == "v0");
    // The current save was backed up, and the chosen oldest was not rotated out.
    const auto backups = ab::list(box.config);
    CHECK(read(backups.front().save(box.config) / "MHP2NDG.BIN") == "current");
    CHECK(fs::exists(oldest.folder));
}

void test_restore_missing_backup_keeps_save() {
    Sandbox box("restore-missing");
    box.save("keep me", "s");
    ab::Entry ghost{box.config.root / "20200101-000000", "20200101-000000"};
    const ab::Result r = ab::restore(box.config, ghost, at(0));
    CHECK(!r.ok && !r.error.empty());
    CHECK(read(box.config.save_folder / "MHP2NDG.BIN") == "keep me");
}

void test_restore_without_summary_removes_stale_one() {
    Sandbox box("restore-nosummary");
    box.save("a", "s");
    fs::remove(box.config.summary_file);
    CHECK(ab::back_up_if_changed(box.config, at(0)).ok);
    box.save("b", "summary b");
    CHECK(ab::restore(box.config, ab::list(box.config).back(), at(1)).ok);
    CHECK(read(box.config.save_folder / "MHP2NDG.BIN") == "a");
    CHECK(!fs::exists(box.config.summary_file));
}

// "Back up, then play" (launcher mods warning): after a backup the save has not
// changed, so pressing it again must skip and not rotate out an older backup.
void test_backup_then_play_skips_unchanged() {
    Sandbox box("backup-then-play");
    box.config.keep = 2;
    box.save("old", "s old");
    CHECK(ab::back_up_if_changed(box.config, at(0)).ok);
    box.save("new", "s new");
    CHECK(ab::back_up(box.config, at(1)).ok);
    for (int i = 2; i < 5; ++i) {
        const ab::Result r = ab::back_up_if_changed(box.config, at(i));
        CHECK(r.ok && r.skipped && r.removed.empty());
    }
    const auto backups = ab::list(box.config);
    CHECK(backups.size() == 2);
    CHECK(read(backups.back().save(box.config) / "MHP2NDG.BIN") == "old");
    box.save("newer", "s newer");
    const ab::Result r = ab::back_up_if_changed(box.config, at(5));
    CHECK(r.ok && !r.skipped && r.removed.size() == 1);
}

} // namespace

int main() {
    test_stamps();
    test_config();
    test_no_save_is_skipped();
    test_backup_only_when_changed();
    test_rotation_keeps_newest_five();
    test_same_second_gets_next_name();
    test_partial_is_cleaned();
    test_restore_backs_up_current_first();
    test_restore_unchanged_makes_no_extra_backup();
    test_restore_oldest_survives_rotation();
    test_restore_missing_backup_keeps_save();
    test_restore_without_summary_removes_stale_one();
    test_backup_then_play_skips_unchanged();
    if (failures == 0) std::cout << "auto_backup_tests: all passed\n";
    return failures == 0 ? 0 : 1;
}
