#include "mods_page.hpp"
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "fu_host_text.hpp"
#include "mod_manager.hpp"
#include "profile.hpp"
#include "ui/layer.hpp"
#include "ui/widgets.hpp"
#include "ui/host_text.hpp"
#include "gpu/vulkan_renderer.hpp"
#include "install/user_data.hpp"
#include "imgui.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdio>
#include <future>
#include <memory>
#include <optional>

namespace portablekit::fu {
namespace {
std::string size_text(std::uint64_t bytes) {
    char text[32];
    if (bytes >= 1024ull * 1024 * 1024) std::snprintf(text, sizeof text, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else std::snprintf(text, sizeof text, "%.1f MB", bytes / (1024.0 * 1024));
    return text;
}

int count_enabled(const std::vector<mods::InstalledMod> &list) {
    int n = 0;
    for (const auto &mod : list) n += mod.enabled ? 1 : 0;
    return n;
}
// Launcher setting texture_dump, kept in <data>\launcher.ini. The renderer
// reads MHFU_TEXTURE_DUMP once when it starts, and it starts before the
// launcher draws, so the switch takes effect the next time the app starts.
constexpr const char *kTextureDumpKey = "texture_dump";
std::filesystem::path launcher_ini() { return install::user_data_directory() / "launcher.ini"; }
} // namespace

bool texture_dump_setting() {
    std::ifstream file(launcher_ini());
    std::string line;
    bool on = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == std::string(kTextureDumpKey) + "=1") on = true;
        else if (line == std::string(kTextureDumpKey) + "=0") on = false;
    }
    return on;
}

bool set_texture_dump_setting(bool on) {
    // Keep any other lines the file holds.
    std::vector<std::string> lines;
    {
        std::ifstream in(launcher_ini());
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind(std::string(kTextureDumpKey) + "=", 0) != 0) lines.push_back(line);
        }
    }
    lines.push_back(std::string(kTextureDumpKey) + (on ? "=1" : "=0"));
    std::ofstream out(launcher_ini(), std::ios::trunc);
    for (const auto &line : lines) out << line << '\n';
    out.flush();
    return static_cast<bool>(out);
}

std::filesystem::path texture_dump_folder() { return install::user_data_directory() / "TextureDump"; }

namespace {
} // namespace

bool show_mods_page() {
    using namespace ui;
    auto &layer = Layer::get();
    enum class Stage { List, Review, Installing, Result };
    Stage stage = Stage::List;
    std::optional<mods::ModPreview> preview;
    auto progress = std::make_shared<mods::InstallProgress>();
    std::future<std::pair<bool, std::string>> job;
    std::string message, error, notice;
    bool result_ok = false;
    std::vector<mods::InstalledMod> list = mods::installed();
    int enabled_count = count_enabled(list);
    bool done = false, refocus = true;
    bool texture_dump = texture_dump_setting();
    std::string dump_error;
    const auto go = [&](Stage next) { stage = next; refocus = true; };

    const bool kept = layer.run([&] {
        const auto cancel = layer.confirm_south() ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown;
        const bool back = layer.take_back() || ImGui::IsKeyPressed(cancel, false);
        // A file dropped on the window opens the install pop-up for it.
        if (auto dropped = layer.take_dropped_file(); dropped && stage != Stage::Installing) {
            preview = mods::inspect(*dropped);
            go(Stage::Review);
        }
        if (stage == Stage::Installing && job.valid() &&
            job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto [ok, why] = job.get();
            if (ok) mods::restore_overlays();
            result_ok = ok;
            message = ok ? ui::host_text::format("launcher.mods.installed_message", {preview->info.name})
                         : ui::host_text::format("launcher.mods.not_installed_message", {why});
            list = mods::installed();
            enabled_count = count_enabled(list);
            for (const auto &mod : list)
                if (ok && mod.enabled && mod.info.id == preview->info.id) {
                    layer.renderer().reload_texture_pack();
                    message = ui::host_text::format("launcher.mods.updated_message", {preview->info.name});
                }
            go(Stage::Result);
        }

        begin_panel("##fu_mods", ui::host_text::cstr("launcher.mods.mods"), game().project_name, false);
        begin_content();
        const bool focus = refocus;
        refocus = false;
        switch (stage) {
        case Stage::List: {
            section(ui::host_text::cstr("launcher.mods.install_a_mod"));
            paragraph(ui::host_text::cstr("launcher.mods.to_install_a_mod_drag"));
            paragraph(ui::host_text::cstr("launcher.mods.installed_mods_are_listed_below"),
                      colors::kTextDim);
            if (list.empty()) {
                section(ui::host_text::cstr("launcher.mods.installed_mods"));
                paragraph(ui::host_text::cstr("launcher.mods.no_mods_installed_yet"), colors::kTextDim);
            }
            bool first_row = focus;
            std::string group;
            bool changed = false;
            for (std::size_t i = 0; i < list.size() && !changed; ++i) {
                const auto &mod = list[i];
                if (mod.info.type != group) {
                    group = mod.info.type;
                    section(ui::host_text::format("launcher.mods.group", {mods::type_label(group)}).c_str());
                }
                if (first_row) { focus_next_row(); first_row = false; }
                std::string label = mod.info.name;
                if (!mod.info.version.empty()) label += "  v" + mod.info.version;
                if (!mod.info.language.empty()) label += "  [" + mod.info.language + "]";
                RowOptions options;
                options.disabled = !mod.problem.empty() && !mod.enabled;
                options.note = !mod.problem.empty() ? ui::host_text::cstr("launcher.mods.cant_be_used")
                               : mod.enabled       ? ui::host_text::format("launcher.mods.on_position", {std::to_string(mod.position + 1)})
                                                   : ui::host_text::cstr("launcher.mods.off");
                options.description = mod.info.description;
                if (toggle_row((label + "##" + mod.info.id).c_str(), mod.enabled, options)) {
                    error.clear();
                    notice.clear();
                    if (mods::set_enabled(mod, !mod.enabled, error, &notice)) layer.renderer().reload_texture_pack();
                    if (mod.info.type == "text") reload_host_text();
                    changed = true;
                    break;
                }
                if (!mod.problem.empty()) paragraph(ui::host_text::format("launcher.mods.cant_be_used_reason", {mod.problem}), colors::kTextDim);
                for (const auto &warning : mod.warnings) paragraph(warning, colors::kDanger);
                if (mod.enabled && mod.problem.empty()) {
                    // Load order: only among mods that are on.
                    const bool can_down = mod.position >= 0 && mod.position + 1 < enabled_count;
                    RowOptions up, down;
                    up.disabled = mod.position <= 0;
                    down.disabled = !can_down;
                    up.description = down.description = ui::host_text::cstr("launcher.mods.changes_the_order_mods_that");
                    if (button_row((std::string("    ") + ui::host_text::cstr("launcher.mods.move_up") + "##up-" + mod.info.id).c_str(), up)) {
                        error.clear();
                        mods::move(mod, -1, error);
                        layer.renderer().reload_texture_pack();
                        if (mod.info.type == "text") reload_host_text();
                        changed = true;
                    } else if (button_row((std::string("    ") + ui::host_text::cstr("launcher.mods.move_down") + "##down-" + mod.info.id).c_str(), down)) {
                        error.clear();
                        mods::move(mod, +1, error);
                        layer.renderer().reload_texture_pack();
                        if (mod.info.type == "text") reload_host_text();
                        changed = true;
                    }
                }
            }
            if (changed) {
                list = mods::installed();
                enabled_count = count_enabled(list);
            }
            if (!notice.empty()) paragraph(notice, colors::kTextDim);
            if (!error.empty()) paragraph(error, colors::kTextDim);            section(ui::host_text::cstr("launcher.mods.texture_dump"));
            {
                RowOptions dump;
                dump.note = texture_dump ? ui::host_text::cstr("launcher.mods.on") : ui::host_text::cstr("launcher.mods.off");
                dump.description = ui::host_text::cstr("launcher.mods.saves_the_games_original_textures");
                if (first_row) { focus_next_row(); first_row = false; }
                if (toggle_row((std::string(ui::host_text::cstr("launcher.mods.dump_textures")) + "##texture_dump").c_str(), texture_dump, dump)) {
                    dump_error.clear();
                    if (set_texture_dump_setting(!texture_dump)) texture_dump = !texture_dump;
                    else dump_error = ui::host_text::cstr("launcher.mods.couldnt_save_the_setting_to");
                }
                paragraph(ui::host_text::format("launcher.mods.texture_dump_help", {install::path_to_utf8(texture_dump_folder())}),
                          colors::kTextDim);
                if (!dump_error.empty()) paragraph(dump_error, colors::kDanger);
            }

            if (first_row) focus_next_row();
            if (button_row(ui::host_text::cstr("launcher.mods.back"))) done = true;
            break;
        }
        case Stage::Review: {
            const auto &p = *preview;
            heading(p.ok() ? p.info.name : ui::host_text::cstr("launcher.mods.cant_install_this_file"));
            if (p.ok()) {
                if (!p.info.version.empty()) info_row(ui::host_text::cstr("launcher.mods.version"), p.info.version);
                if (!p.info.author.empty()) info_row(ui::host_text::cstr("launcher.mods.author"), p.info.author);
                info_row(ui::host_text::cstr("launcher.mods.type"), p.info.type == "texture" ? ui::host_text::cstr("launcher.mods.texture_pack") : mods::type_label(p.info.type));
                if (!p.info.language.empty()) info_row(ui::host_text::cstr("launcher.mods.language"), p.info.language);
                info_row(ui::host_text::cstr("launcher.mods.size"), ui::host_text::format("launcher.mods.size_value", {size_text(p.bytes), std::to_string(p.files)}));
                if (!p.info.description.empty()) paragraph(p.info.description);
                if (!p.info.has_manifest)
                    paragraph(ui::host_text::cstr("launcher.mods.this_zip_has_no_mod"), colors::kTextDim);
                if (p.info.affects_save)
                    paragraph(ui::host_text::cstr("launcher.mods.this_mod_changes_your_save"), colors::kDanger);
                if (p.replaces)
                    paragraph(p.installed_version.empty() ? std::string(ui::host_text::cstr("launcher.mods.this_replaces_the_installed_copy"))
                                                          : ui::host_text::format("launcher.mods.replaces_version", {p.installed_version}),
                              colors::kTextDim);
                if (focus) focus_next_row();
                if (button_row(ui::host_text::cstr("launcher.mods.install"))) {
                    progress = std::make_shared<mods::InstallProgress>();
                    job = std::async(std::launch::async, [p, progress] {
                        std::string why;
                        const bool ok = mods::install(p, *progress, why);
                        return std::make_pair(ok, why);
                    });
                    go(Stage::Installing);
                }
                if (button_row(ui::host_text::cstr("launcher.mods.cancel"))) go(Stage::List);
            } else {
                paragraph(p.problem);
                if (focus) focus_next_row();
                if (button_row(ui::host_text::cstr("launcher.mods.ok"))) go(Stage::List);
            }
            break;
        }
        case Stage::Installing: {
            heading(ui::host_text::format("launcher.mods.installing", {preview->info.name}));
            const auto total = progress->total.load();
            progress_bar(total ? static_cast<float>(progress->bytes.load()) / total : 0.0f,
                         ui::host_text::format("launcher.mods.progress", {size_text(progress->bytes.load()), size_text(total)}));
            break;
        }
        case Stage::Result:
            heading(result_ok ? ui::host_text::cstr("launcher.mods.installed") : ui::host_text::cstr("launcher.mods.not_installed"));
            paragraph(message);
            if (focus) focus_next_row();
            if (button_row(ui::host_text::cstr("launcher.mods.ok"))) go(Stage::List);
            break;
        }
        begin_footer();
        hints({{Control::Confirm, ui::host_text::cstr("launcher.mods.select")}, {Control::Back, ui::host_text::cstr("launcher.mods.back")}});
        end_panel();
        if (back) {
            if (stage == Stage::List) done = true;
            else if (stage != Stage::Installing) go(Stage::List);
        }
        return !done;
    }, false);
    if (job.valid()) job.wait();
    return kept;
}
} // namespace portablekit::fu

namespace {
// Each enabled text mod's font/ folder is a small texture pack the renderer
// checks before the texture mod; reload_texture_pack() re-reads this list.
const bool kFontPacksRegistered = [] {
    portablekit::gpu::VulkanRenderer::set_font_pack_source([] {
        std::vector<std::filesystem::path> folders;
        for (const portablekit::fu::mods::TextMod &mod : portablekit::fu::mods::enabled_text_mods()) {
            std::error_code ec;
            if (std::filesystem::is_directory(mod.font, ec)) folders.push_back(mod.font);
        }
        return folders;
    });
    return true;
}();

// Texture dump: set up before the renderer starts, which reads
// MHFU_TEXTURE_DUMP once. A value the player set by hand wins. The texture mod
// is skipped for this session through MHFU_TEXTURE_PACK=0, a settings override
// that settings::save() never writes back, so enabled.txt and settings.ini
// keep the player's choice.
const bool kTextureDumpPrepared = [] {
    try {
        if (!portablekit::fu::texture_dump_setting()) return false;
        const auto set = [](const char *name, const std::string &value) {
            const std::string full = portablekit::env_name(name);
#if defined(_WIN32)
            _putenv_s(full.c_str(), value.c_str());
#else
            setenv(full.c_str(), value.c_str(), 0);
#endif
        };
        if (const char *own = portablekit::env("TEXTURE_DUMP"); own == nullptr || *own == '\0') {
            const std::filesystem::path folder = portablekit::fu::texture_dump_folder();
            std::error_code ec;
            std::filesystem::create_directories(folder, ec);
            if (ec) {
                std::cerr << "[texdump] cannot create " << portablekit::install::path_to_utf8(folder) << ": "
                          << ec.message() << "\n";
                return false;
            }
            set("TEXTURE_DUMP", portablekit::install::path_to_utf8(folder));
        }
        if (portablekit::env("TEXTURE_PACK") == nullptr) set("TEXTURE_PACK", "0");
        std::cout << "[texdump] on: dumping to " << portablekit::env("TEXTURE_DUMP") << "; texture mod skipped\n";
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[texdump] not started: " << e.what() << "\n";
        return false;
    }
}();
} // namespace
#else
namespace portablekit::fu { bool show_mods_page() { return true; } }
#endif
