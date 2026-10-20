// <data>\launcher.ini: launcher-only settings, one key=value per line. Shared by
// the Mods page (texture_dump), the updater (update_dismissed) and the bug
// report. Unknown lines are kept on write.
#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace portablekit::fu::launcher_ini {
[[nodiscard]] std::filesystem::path path();
[[nodiscard]] std::optional<std::string> get(const std::string &key);
bool set(const std::string &key, const std::string &value);
} // namespace portablekit::fu::launcher_ini
