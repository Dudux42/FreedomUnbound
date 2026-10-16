#include "bug_report.hpp"

#include "launcher_ini.hpp"
#include "mod_manager.hpp"

#include "install/user_data.hpp"

#include <miniz.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <optional>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

#if defined(PORTABLEKIT_HAS_RENDERER)
#include "ui/layer.hpp"
#include "ui/save_screen.hpp"  // ui::open_folder
#include "ui/widgets.hpp"
#include "ui/host_text.hpp"
#include "imgui.h"
#include <future>
#endif

#ifndef MHFU_VERSION
#define MHFU_VERSION "0.0.0-dev"
#endif

namespace portablekit::fu {
namespace {
namespace fs = std::filesystem;

constexpr std::size_t kMaxLogs = 6;                     // newest first
constexpr std::size_t kMaxSessions = 3;                 // newest launcher sessions
constexpr std::uintmax_t kMaxLogBytes = 4u * 1024 * 1024;  // tail of each log

// settings.ini keys whose values are personal: replaced, key kept.
constexpr const char *kPersonalKeys[] = {
    "disc_image", "input.name", "network.mac", "network.nickname", "network.server",
    "network.recent", "ui.last_folder", "video.texture_pack_folder",
};

std::string read_file(const fs::path &path, std::uintmax_t max_tail = 0) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    if (max_tail != 0) {
        std::error_code ec;
        const auto size = fs::file_size(path, ec);
        if (!ec && size > max_tail) in.seekg(static_cast<std::streamoff>(size - max_tail));
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char text[32];
    std::strftime(text, sizeof text, "%Y%m%d-%H%M%S", &local);
    return text;
}

fs::path logs_directory() {
#ifdef _WIN32
    // launcher_entry.cpp's default: %LOCALAPPDATA%\FreedomUnbound\logs.
    wchar_t *local = nullptr;
    fs::path base;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local) == S_OK) base = local;
    if (local) CoTaskMemFree(local);
    return base.empty() ? fs::path() : base / L"FreedomUnbound" / L"logs";
#else
    return install::user_data_directory() / "logs";
#endif
}

std::string os_version() {
#ifdef _WIN32
    // GetVersionEx lies after 8.1; RtlGetVersion does not.
    using RtlGetVersionFn = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
        if (auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))) fn(&info);
    char text[96];
    std::snprintf(text, sizeof text, "Windows %lu.%lu build %lu", info.dwMajorVersion, info.dwMinorVersion,
                  info.dwBuildNumber);
    return text;
#else
    return "non-Windows";
#endif
}

bool add(mz_zip_archive &zip, const std::string &name, const std::string &data) {
    return mz_zip_writer_add_mem(&zip, name.c_str(), data.data(), data.size(), MZ_BEST_COMPRESSION) != 0;
}

void fix_json_trailing_commas(std::vector<std::string> &lines) {
    const auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
        std::string &prev = lines[i];
        std::size_t end = prev.size();
        while (end > 0 && is_space(prev[end - 1])) --end;
        if (end == 0 || prev[end - 1] != ',') continue;
        const std::string &next = lines[i + 1];
        std::size_t start = 0;
        if (next.compare(0, 3, "\xEF\xBB\xBF") == 0) start = 3;
        while (start < next.size() && is_space(next[start])) ++start;
        if (start < next.size() && (next[start] == '}' || next[start] == ']')) prev.erase(end - 1, 1);
    }
}

std::string scrub_text(const std::string &text, bool json = false) {
    std::istringstream in(text);
    std::vector<std::string> kept;
    std::string line;
    while (std::getline(in, line))
        if (!drop_line(line)) kept.push_back(scrub_line(line));
    // FU Controls: dropping the last member of a JSON object (session.json's
    // "executable_sha256") leaves the line before it ending in ',', right in
    // front of '}' or ']'. Strip that comma so the file still parses.
    if (json) fix_json_trailing_commas(kept);
    std::string out;
    for (const std::string &k : kept) out += k + '\n';
    return out;
}

// Never read, whatever the folder: saves, Data Install, Import leftovers,
// automatic save backups (FU Saves, item 4) and dumped textures.
bool excluded_path(const fs::path &path) {
    for (const fs::path &part : path) {
        std::string p = install::path_to_utf8(part);
        std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (p == "savedata" || p == "savebackups" || p == "texturedump" || p == "ms0" ||
            p == "ulus10391dat") return true;
    }
    std::string ext = install::path_to_utf8(path.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".backup";
}

// ---- FU Controls: which log files go into the report -----------------------
// 1. Launcher sessions: <logs root>\<YYYYMMDD-HHMMSS-mmm>-<pid>\ with
//    stdout.log, stderr.log and session.json; the kMaxSessions newest (by
//    folder name, which starts with the time; mtime breaks ties), as
//    logs/<session>/<file>.
// 2. Loose .log/.txt files in the logs root and in <data>\logs (the in-game
//    FreedomUnbound-*.log among them): the kMaxLogs newest, as logs/<file>.
// excluded_path() applies to every file and folder.
struct LogFile {
    std::string zip_name;
    fs::path path;
};
std::vector<LogFile> collect_logs(const fs::path &session_root, const fs::path &data_logs) {
    std::vector<LogFile> out;
    std::error_code ec;
    const auto newer = [](const fs::directory_entry &a, const fs::directory_entry &b) {
        std::error_code e;
        return a.last_write_time(e) > b.last_write_time(e);
    };
    if (!session_root.empty() && fs::is_directory(session_root, ec)) {
        std::vector<fs::directory_entry> sessions;
        for (const auto &entry : fs::directory_iterator(session_root, ec))
            if (entry.is_directory(ec) && !excluded_path(entry.path())) sessions.push_back(entry);
        std::sort(sessions.begin(), sessions.end(), [&](const auto &a, const auto &b) {
            const std::string an = install::path_to_utf8(a.path().filename());
            const std::string bn = install::path_to_utf8(b.path().filename());
            return an != bn ? an > bn : newer(a, b);
        });
        std::size_t taken = 0;
        for (const auto &session : sessions) {
            if (taken == kMaxSessions) break;
            bool any = false;
            for (const char *name : {"stdout.log", "stderr.log", "session.json"}) {
                const fs::path file = session.path() / name;
                if (!fs::is_regular_file(file, ec) || excluded_path(file)) continue;
                out.push_back({"logs/" + install::path_to_utf8(session.path().filename()) + "/" + name, file});
                any = true;
            }
            if (any) ++taken;  // empty session folders don't use up a slot
        }
    }
    std::vector<fs::directory_entry> loose;
    for (const fs::path &dir : {session_root, data_logs}) {
        if (dir.empty() || !fs::is_directory(dir, ec)) continue;
        for (const auto &entry : fs::directory_iterator(dir, ec))
            if (entry.is_regular_file(ec) && !excluded_path(entry.path()) &&
                (entry.path().extension() == ".log" || entry.path().extension() == ".txt"))
                loose.push_back(entry);
    }
    std::sort(loose.begin(), loose.end(), newer);
    if (loose.size() > kMaxLogs) loose.resize(kMaxLogs);
    for (const auto &entry : loose) out.push_back({"logs/" + install::path_to_utf8(entry.path().filename()), entry.path()});
    return out;
}

} // namespace

bool drop_line(const std::string &line) {
    // A log from a session with MHFU_TRACE_SAVEDATA holds the save key as
    // plain hex: drop every savedata line and any line with a 32+ hex run.
    static const std::regex kSavedata("savedata", std::regex::icase);
    static const std::regex kLongHex("[0-9A-Fa-f]{32,}");
    return std::regex_search(line, kSavedata) || std::regex_search(line, kLongHex);
}

std::string scrub_line(const std::string &line) {
    // key=..., token: ..., "secret": "...", Bearer ..., and long hex/base64 runs.
    static const std::regex kAssign(
        R"((\b[\w.\-]*(?:key|token|secret|password|passwd|auth|cookie|session)[\w.\-]*\b\s*["']?\s*[=:]\s*["']?)([^\s"',;]+))",
        std::regex::icase);
    static const std::regex kBearer(R"((\bBearer\s+)\S+)", std::regex::icase);
    // No \b: "0x" prefixes and hex glued to letters are caught too.
    static const std::regex kHex("[0-9A-Fa-f]{16,}");
    static const std::regex kBase64(R"([A-Za-z0-9+/]{40,}={0,2})");
    // C:\Users\<name>\ and /home/<name>/ -> <user>
    static const std::regex kUser(R"(((?:[A-Za-z]:)?[\\/](?:Users|home)[\\/])[^\\/\s"]+)", std::regex::icase);
    std::string out = std::regex_replace(line, kBearer, "$1<redacted>");
    out = std::regex_replace(out, kAssign, "$1<redacted>");
    // [texpack]/[texdump] lines (written bare at the start of a line by
    // texture_pack.cpp, vulkan_renderer.cpp, mods_page.cpp) keep their texture
    // hashes and addresses: modders need them. Leading whitespace and a plain
    // or bracketed timestamp are allowed in front. drop_line() still applies.
    static const std::regex kTexTag(R"(^\s*(?:[0-9:.\-T/ ]+\s|\[[0-9:.\-T ]+\]\s*)?\[tex(?:pack|dump)\])");
    if (!std::regex_search(out, kTexTag)) out = std::regex_replace(out, kHex, "<hex redacted>");
    out = std::regex_replace(out, kBase64, "<redacted>");
    out = std::regex_replace(out, kUser, "$1<user>");
    return out;
}

std::string scrub_settings(const std::string &text) {
    std::istringstream in(text);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t eq = line.find('=');
        if (eq != std::string::npos && line[0] != '#') {
            const std::string key = line.substr(0, eq);
            const bool personal = std::any_of(std::begin(kPersonalKeys), std::end(kPersonalKeys),
                                              [&](const char *k) { return key == k; });
            if (personal) {
                out << key << "=<redacted" << (eq + 1 < line.size() ? "" : ", empty") << ">\n";
                continue;
            }
        }
        if (!drop_line(line)) out << scrub_line(line) << '\n';
    }
    return out.str();
}

BugReportResult create_bug_report() {
    BugReportResult result;
    const fs::path data = install::user_data_directory();
    const fs::path folder = data / "BugReports";
    std::error_code ec;
    fs::create_directories(folder, ec);
    result.zip = folder / ("FreedomUnbound-report-" + timestamp() + ".zip");

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, result.zip.string().c_str(), 0)) {
        result.error = "Could not create " + install::path_to_utf8(result.zip);
        return result;
    }
    bool ok = true;
    const auto put = [&](const std::string &name, const std::string &text) {
        ok = add(zip, name, text) && ok;
        ++result.files;
    };

    // system.txt
    {
        std::ostringstream s;
        s << "Freedom Unbound v" << MHFU_VERSION << "\n" << os_version() << "\n";
        s << "Report created " << timestamp() << " (local time)\n";
        s << "Not included: saves (the PSP save folders, Data Install, *.backup), save backups, save-preview, keys.txt, "
             "EBOOT.ELF, disc image, TextureDump, caches. Save-related log lines and long hex runs are dropped.\n";
        put("system.txt", s.str());
    }
    // Settings, scrubbed. settings.ini holds paths, names and the ad hoc MAC.
    if (fs::is_regular_file(data / install::kSettingsFile, ec))
        put("settings.ini", scrub_settings(read_file(data / install::kSettingsFile)));
    if (fs::is_regular_file(launcher_ini::path(), ec)) put("launcher.ini", scrub_text(read_file(launcher_ini::path())));
    if (fs::is_regular_file(data / "cheats.ini", ec)) put("cheats.ini", scrub_text(read_file(data / "cheats.ini")));
    if (fs::is_regular_file(mods::mods_root() / "enabled.txt", ec))
        put("mods/enabled.txt", read_file(mods::mods_root() / "enabled.txt"));
    // FU Saves: the save-touching mod list (optional). The save summary is left out.
    if (fs::is_regular_file(data / "mods-used.txt", ec)) put("mods-used.txt", scrub_text(read_file(data / "mods-used.txt")));
    // Installed mods: metadata only, never their files.
    {
        std::ostringstream m;
        m << "id\tversion\ttype\tenabled\taffects_save\tproblem\n";
        for (const auto &mod : mods::installed())
            m << mod.info.id << '\t' << mod.info.version << '\t' << mod.info.type << '\t' << (mod.enabled ? "on" : "off")
              << '\t' << (mod.info.affects_save ? "yes" : "no") << '\t' << mod.problem << '\n';
        put("mods/installed.tsv", m.str());
    }
    // The newest logs, tail-capped and scrubbed.
    for (const LogFile &log : collect_logs(logs_directory(), data / "logs"))
        put(log.zip_name, scrub_text(read_file(log.path, kMaxLogBytes), log.path.extension() == ".json"));
    ok = mz_zip_writer_finalize_archive(&zip) && ok;
    mz_zip_writer_end(&zip);
    if (!ok) {
        fs::remove(result.zip, ec);
        result.error = "Could not write the report zip.";
        return result;
    }
    result.ok = true;
    return result;
}

#if defined(PORTABLEKIT_HAS_RENDERER)
bool show_bug_report_page() {
    using namespace ui;
    Layer &layer = Layer::get();
    bool done = false;
    bool first = true;
    std::future<BugReportResult> pending;
    std::optional<BugReportResult> last;
    return layer.run(
        [&] {
            if (layer.take_back()) done = true;
            if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                last = pending.get();
                if (last->ok) (void)open_folder(last->zip.parent_path());
            }
            begin_panel("##bug_report", ui::host_text::cstr("launcher.report.report_a_bug"), "Freedom Unbound", false);
            begin_content();
            paragraph(ui::host_text::cstr("launcher.report.creates_a_zip_with_your"), colors::kTextDim);
            ImGui::Dummy({0.0f, ImGui::GetFontSize() * 0.4f});
            if (first) focus_next_row();
            first = false;
            const bool busy = pending.valid();
            if (button_row(busy ? ui::host_text::cstr("launcher.report.creating_report") : ui::host_text::cstr("launcher.report.create_bug_report"),
                           {busy, {}, ui::host_text::cstr("launcher.report.writes_the_zip_to_the")}))
                pending = std::async(std::launch::async, create_bug_report);
            if (last) {
                // A status line, not a control: keep it off pad/arrow-key
                // navigation, like the launcher's update status (FU Controls).
                ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
                if (last->ok) info_row(ui::host_text::cstr("launcher.report.saved"), install::path_to_utf8(last->zip.filename()));
                else info_row(ui::host_text::cstr("launcher.report.failed"), last->error);
                ImGui::PopItemFlag();
            }
            if (button_row(ui::host_text::cstr("launcher.report.open_reports_folder"), {false, {}, ui::host_text::cstr("launcher.report.show_the_bugreports_folder")})) {
                std::error_code ec;
                const auto dir = install::user_data_directory() / "BugReports";
                fs::create_directories(dir, ec);
                (void)open_folder(dir);
            }
            if (kDiscordInviteUrl[0] != '\0' &&
                button_row(ui::host_text::cstr("launcher.report.open_discord"), {false, {}, ui::host_text::cstr("launcher.report.post_the_zip_in_the")})) {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", kDiscordInviteUrl, nullptr, nullptr, SW_SHOWNORMAL);
#endif
            }
            if (button_row(ui::host_text::cstr("launcher.report.back"), {false, {}, ui::host_text::cstr("launcher.report.back_to_the_home_screen")})) done = true;
            begin_footer();
            hints({{Control::Confirm, ui::host_text::cstr("launcher.report.select")}, {Control::Back, ui::host_text::cstr("launcher.report.back")}});
            end_panel();
            return !done;
        },
        false);
}
#else
bool show_bug_report_page() { return true; }
#endif

} // namespace portablekit::fu
