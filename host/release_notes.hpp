// Release notes for "What's new" (FU Controls): no UI, no network, so the
// tests can build it alone.
#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace portablekit::fu::release_notes {
// The JSON string value of "key" in `json`, unescaped (\n, \", \\, \uXXXX to
// UTF-8, surrogate pairs included). Empty if missing or null.
[[nodiscard]] std::string json_text(const std::string &json, const std::string &key);
// "v0.1.1-alpha" and "0.1.1-alpha" are the same version.
[[nodiscard]] bool same_version(const std::string &a, const std::string &b);
// <data>\whatsnew-cache.txt: first line the tag, then the notes.
bool write_cache(const std::string &tag, const std::string &notes);
bool write_cache_to(const std::filesystem::path &file, const std::string &tag, const std::string &notes);
// The cached notes if they are for `version`.
[[nodiscard]] std::optional<std::string> read_cache_from(const std::filesystem::path &file, const std::string &version);
[[nodiscard]] std::optional<std::string> read_cache(const std::string &version);
// WHATSNEW.md beside the program, bundled by the package script.
[[nodiscard]] std::optional<std::string> read_bundled(const std::filesystem::path &program_dir);
// launcher.ini's last_seen_version against this build: true when the notes
// should open by themselves. A missing key counts as an update, so players
// coming from a build without this page see it once too.
[[nodiscard]] bool due(const std::optional<std::string> &last_seen, const std::string &version);
} // namespace portablekit::fu::release_notes
