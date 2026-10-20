#include "whats_new.hpp"

#include "bug_report.hpp"  // kDiscordInviteUrl
#include "launcher_ini.hpp"
#include "release_notes.hpp"
#include "updater.hpp"

#include "install/user_data.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

#if defined(PORTABLEKIT_HAS_RENDERER)
#include "ui/layer.hpp"
#include "ui/widgets.hpp"
#include "ui/host_text.hpp"
#include "imgui.h"
#endif

#ifndef MHFU_VERSION
#define MHFU_VERSION "0.0.0-dev"
#endif

namespace portablekit::fu {
namespace {
namespace fs = std::filesystem;
constexpr const char *kSeenKey = "last_seen_version";
constexpr const char *kReleasePage = "https://github.com/Dudux42/FreedomUnbound/releases/tag/v";

fs::path program_dir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (n > 0 && n < std::size(buffer)) return fs::path(buffer).parent_path();
#endif
    return fs::current_path();
}

void open_url(const char *url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)url;
#endif
}

// Where the notes on screen came from, best first.
enum class Source { None, Release, Cache, Bundled };
struct Notes {
    Source source{Source::None};
    std::string text;
};
Notes local_notes() {
    if (auto cached = release_notes::read_cache(MHFU_VERSION)) return {Source::Cache, *cached};
    if (auto bundled = release_notes::read_bundled(program_dir())) return {Source::Bundled, *bundled};
    return {};
}
} // namespace

bool whats_new_due() { return release_notes::due(launcher_ini::get(kSeenKey), MHFU_VERSION); }

#if defined(PORTABLEKIT_HAS_RENDERER)
bool show_whats_new_page() {
    using namespace ui;
    Layer &layer = Layer::get();
    (void)launcher_ini::set(kSeenKey, MHFU_VERSION);
    ensure_update_check();  // never downloads; fills in the release's notes
    Notes notes = local_notes();
    bool done = false;
    bool first = true;
    return layer.run(
        [&] {
            if (layer.take_back()) done = true;
            if (notes.source != Source::Release) {
                std::string tag, text;
                if (current_release_notes(tag, text)) notes = {Source::Release, text};
            }
            begin_panel("##whats_new", ui::host_text::cstr("launcher.whats_new.whats_new"), std::string("Freedom Unbound v") + MHFU_VERSION, false);
            begin_content();
            if (notes.text.empty()) {
                paragraph(ui::host_text::cstr("launcher.whats_new.no_notes_for_this_version"),
                          colors::kTextDim);
            } else {
                // Markdown, lightly: headings in the accent colour, the rest as text.
                std::istringstream in(notes.text);
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) {
                        ImGui::Dummy({0.0f, ImGui::GetFontSize() * 0.3f});
                        continue;
                    }
                    if (line[0] == '#') {
                        const size_t start = line.find_first_not_of("# ");
                        paragraph(start == std::string::npos ? std::string() : line.substr(start),
                                  colors::kAccentBright);
                        continue;
                    }
                    if (line.size() > 2 && (line[0] == '*' || line[0] == '-') && line[1] == ' ') line[0] = '-';
                    paragraph(line);
                }
            }
            ImGui::Dummy({0.0f, ImGui::GetFontSize() * 0.4f});
            if (first) focus_next_row();
            first = false;
            if (button_row(ui::host_text::cstr("launcher.whats_new.continue"), {false, {}, ui::host_text::cstr("launcher.whats_new.back_to_the_home_screen")})) done = true;
            if (button_row(ui::host_text::cstr("launcher.whats_new.open_release_page"), {false, {}, ui::host_text::cstr("launcher.whats_new.this_versions_page_on_github")}))
                open_url((std::string(kReleasePage) + MHFU_VERSION).c_str());
            if (kDiscordInviteUrl[0] != '\0' &&
                button_row(ui::host_text::cstr("launcher.whats_new.open_discord"), {false, {}, ui::host_text::cstr("launcher.whats_new.news_help_and_bug_reports")}))
                open_url(kDiscordInviteUrl);
            begin_footer();
            hints({{Control::Confirm, ui::host_text::cstr("launcher.whats_new.select")}, {Control::Back, ui::host_text::cstr("launcher.whats_new.back")}});
            end_panel();
            return !done;
        },
        false);
}
#else
bool show_whats_new_page() {
    (void)launcher_ini::set(kSeenKey, MHFU_VERSION);
    return true;
}
#endif

} // namespace portablekit::fu
