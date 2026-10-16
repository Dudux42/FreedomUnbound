// Points PortableKit's host string table (ui/host_text.hpp) at the host.txt
// files for the chosen language, so the launcher, the settings menu and the
// save dialogs can be translated. The table re-asks about once a second, so
// switching a text mod on or off applies without a restart.
//
// Language (FU Controls): settings.ini ui.language is "auto", "en" or
// "pt-BR". Auto takes the language of the first enabled text mod that names
// one (mod.json "language"), otherwise the system's display language. For any
// language but English, in order, later files winning:
//   1. <program folder>/lang/<language>/host.txt, shipped with a release;
//   2. each enabled text mod's text/host.txt whose language matches (a mod
//      that names no language always applies), in load order.
// English uses only the mods that name no language or English, so it shows
// the built-in English text.
#include "fu_host_text.hpp"
#include "mod_manager.hpp"
#include "ui/host_text.hpp"

#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace portablekit::fu {
namespace {

// "pt", "pt-BR", "pt_br"... -> "pt-BR"; "en..." -> "en"; anything else as it is.
std::string normalise(const std::string &language) {
    if (language.size() >= 2 && (language[0] == 'p' || language[0] == 'P') && (language[1] == 't' || language[1] == 'T'))
        return "pt-BR";
    if (language.size() >= 2 && (language[0] == 'e' || language[0] == 'E') && (language[1] == 'n' || language[1] == 'N'))
        return "en";
    return language;
}

std::filesystem::path program_dir() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n == 0) return {};
        if (n < buffer.size()) {
            buffer.resize(n);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::error_code ec;
    return std::filesystem::current_path(ec);
#endif
}

std::string effective_language(const std::vector<mods::TextMod> &enabled) {
    const std::string setting = ui::host_text::language_setting();
    if (setting == "en" || setting == "pt-BR") return setting;
    for (const mods::TextMod &mod : enabled)
        if (!mod.language.empty()) return normalise(mod.language);
    return ui::host_text::system_language();
}

std::vector<std::filesystem::path> host_text_files() {
    const std::vector<mods::TextMod> enabled = mods::enabled_text_mods();
    const std::string language = effective_language(enabled);
    ui::host_text::set_effective_language(language);  // the launcher's flags highlight it
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    if (language != "en") {
        const std::filesystem::path bundled = program_dir() / "lang" / language / "host.txt";
        if (std::filesystem::is_regular_file(bundled, ec)) files.push_back(bundled);
    }
    for (const mods::TextMod &mod : enabled) {
        if (!mod.language.empty() && normalise(mod.language) != language) continue;
        if (!mod.host.empty() && std::filesystem::is_regular_file(mod.host, ec)) files.push_back(mod.host);
    }
    return files;
}

const bool kRegistered = [] {
    ui::host_text::set_provider(host_text_files);
    return true;
}();

} // namespace

// Re-reads the host.txt files now (e.g. right after a toggle).
void reload_host_text() { ui::host_text::reload(); }

} // namespace portablekit::fu
