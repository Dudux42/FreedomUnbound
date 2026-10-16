// Launcher "Report a bug" page (FU Controls): writes
// <data>\BugReports\FreedomUnbound-report-YYYYMMDD-HHMMSS.zip and opens the
// folder. Allow-list only: recent logs, settings.ini, launcher.ini, cheats.ini,
// mods\enabled.txt, mods-used.txt, save-preview\ULUS10391.txt, a mod list (ids + versions) and a system.txt with the app
// and Windows versions. Never ms0 / SAVEDATA (incl. ULUS10391DAT, *.backup),
// SaveBackups, TextureDump, keys.txt,
// EBOOT.ELF, disc.iso or caches. Personal values (paths, names, MAC, server)
// and anything that looks like a key or token are redacted.
#pragma once
#include <filesystem>
#include <string>

namespace portablekit::fu {

// Official Wyvern Works invite (permanent, no use limit). Empty hides the Open Discord button.
inline constexpr const char *kDiscordInviteUrl = "https://discord.gg/UFmAD3s2pH";

struct BugReportResult {
    bool ok{};
    std::filesystem::path zip;
    std::string error;
    int files{};
};
// Builds the zip. Runs on any thread; touches no game state.
[[nodiscard]] BugReportResult create_bug_report();
// Text scrubbing, exposed for tests.
[[nodiscard]] std::string scrub_line(const std::string &line);
// True for a log line that is left out whole (savedata, 32+ hex run).
[[nodiscard]] bool drop_line(const std::string &line);
[[nodiscard]] std::string scrub_settings(const std::string &text);

// The launcher page (GameProfile::launcher_report_page). False: window closed.
bool show_bug_report_page();

} // namespace portablekit::fu
