#define MHFU_GAME_LOG_PURE 1
#include "game_log.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace portablekit::fu::game_log;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

int main() {
    // filter
    CHECK(keep_line("[audio] sceAudioOutput2 ch=0 vol=0x8000"));
    CHECK(keep_line("[fu-text] U+00E1 pinned to cell 96 (text object 0x09971400)"));
    CHECK(keep_line(""));
    CHECK(!keep_line("[hle] sceUtilitySavedataInitStart"));
    CHECK(!keep_line("SAVEDATA dir ms0:/PSP/SAVEDATA/ULUS10391"));
    CHECK(!keep_line("savedata"));
    CHECK(!keep_line("key " + std::string(32, 'a')));
    CHECK(!keep_line("0x" + std::string(16, 'F') + std::string(16, '0')));
    CHECK(keep_line("hash " + std::string(31, 'c') + " end"));
    CHECK(keep_line(std::string(31, '1') + "-" + std::string(31, '2')));
    CHECK(keep_line("[texpack] 00000000ecbb8936 font"));
    CHECK(keep_line("save data"));  // only the word as written

    // names
    std::tm t{};
    t.tm_year = 126; t.tm_mon = 9; t.tm_mday = 13; t.tm_hour = 23; t.tm_min = 5; t.tm_sec = 9;
    CHECK(file_name(t) == "FreedomUnbound-20261013-230509.log");

    // rotation, pure
    std::vector<std::string> names{"launcher.ini", "adhoc-20261013-1.log", "FreedomUnbound-.log"};
    for (int i = 1; i <= 12; ++i) {
        char n[64];
        std::snprintf(n, sizeof(n), "FreedomUnbound-202610%02d-120000.log", i);
        names.push_back(n);
    }
    names.push_back("FreedomUnbound-20261012-120000-2.log");
    auto doomed = files_to_delete(names);
    CHECK(doomed.size() == 3u);
    CHECK(doomed.size() == 3u && doomed[0] == "FreedomUnbound-20261001-120000.log" &&
          doomed[2] == "FreedomUnbound-20261003-120000.log");
    CHECK(files_to_delete({"FreedomUnbound-20261001-120000.log"}).empty());

    // rotation on disk
    const auto dir = std::filesystem::temp_directory_path() / "mhfu_game_log_tests";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    for (const auto &n : names) std::ofstream(dir / n) << "x";
    CHECK(rotate(dir) == 3u);
    std::size_t ours = 0, others = 0;
    for (const auto &e : std::filesystem::directory_iterator(dir)) {
        const std::string n = e.path().filename().string();
        (log_files({n}).empty() ? others : ours)++;
    }
    CHECK(ours == 10u);
    CHECK(others == 3u);  // other files untouched
    CHECK(!std::filesystem::exists(dir / "FreedomUnbound-20261001-120000.log"));
    CHECK(std::filesystem::exists(dir / "FreedomUnbound-20261012-120000-2.log"));
    CHECK(rotate(dir) == 0u);
    std::filesystem::remove_all(dir);

    std::printf(failures ? "%d failure(s)\n" : "game_log tests passed\n", failures);
    return failures ? 1 : 0;
}
