#pragma once

// <data>/mods-used.txt (FU Saves, item 11): the mods that were on when the
// save was last written or loaded, so the launcher can warn before Play when
// the mods that change saves are not the same any more.
//
//   # comment lines
//   FUMODSUSED 1
//   <id>\t<version>\t<affects_save 0|1>\t<name>
//
// Pure functions plus a file read and an atomic write; tests in
// tests/mods_used_tests.cpp.
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace portablekit::fu::mods_used {

struct Mod {
    std::string id, version, name;
    bool affects_save{};
};

[[nodiscard]] std::string format(const std::vector<Mod> &mods);
// Empty optional: not this file's format.
[[nodiscard]] std::optional<std::vector<Mod>> parse(const std::string &text);

[[nodiscard]] std::filesystem::path file_for(const std::filesystem::path &memory_stick);
[[nodiscard]] std::optional<std::vector<Mod>> read(const std::filesystem::path &file);
bool write(const std::filesystem::path &file, const std::vector<Mod> &mods);

// What changed among the mods that change saves: "Name 1.0" entries.
struct Difference {
    std::vector<std::string> added, removed;
    [[nodiscard]] bool any() const { return !added.empty() || !removed.empty(); }
};
// A mod counts by id and version. With no record (nullopt), every current
// save-changing mod counts as added.
[[nodiscard]] Difference compare(const std::optional<std::vector<Mod>> &recorded, const std::vector<Mod> &current);

} // namespace portablekit::fu::mods_used
