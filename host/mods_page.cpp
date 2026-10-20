#include "mods_page.hpp"
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "fu_host_text.hpp"
#include "mod_manager.hpp"
#include "mod_overlay.hpp"
#include "mod_catalog.hpp"
#include "updater.hpp"
#include "profile.hpp"
#include "ui/layer.hpp"
#include "ui/widgets.hpp"
#include "ui/text_input.hpp"
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
#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>

namespace portablekit::fu {
bool game_mods_frame(bool first_frame) {
    static std::vector<mods::InstalledMod> list;
    static std::string message;
    if (first_frame) { list = mods::installed(); message.clear(); }
    const auto &io = ImGui::GetIO();
    const float scale = ui::Layer::get().scale();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, io.DisplaySize.y * .5f), ImGuiCond_Always, ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x - 24.f, 850.f * scale),
                                  std::min(io.DisplaySize.y - 24.f, 620.f * scale)), ImGuiCond_Always);
    bool open = true, refresh = false;
    if (ImGui::Begin("Mod Manager", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextWrapped("F9, Esc or the controller's cancel button closes this menu. The game keeps running while it is open.");
        ImGui::Separator();
        ImGui::BeginChild("installed-mods", ImVec2(0, -105.f * scale));
        if (list.empty()) ImGui::TextWrapped("No mods installed. Install mod ZIPs from the launcher's Mods page.");
        for (const auto &mod : list) {
            ImGui::PushID(mod.info.id.c_str());
            bool on = mod.enabled;
            const bool live = !mod.info.affects_save &&
                (mod.info.type == "overlay" || mod.info.type == "camera" || mod.info.type == "qol");
            ImGui::BeginDisabled(!live || (!on && !mod.problem.empty()));
            if (ImGui::Checkbox(mod.info.name.c_str(), &on)) {
                std::string error, notice;
                mods::set_enabled(mod, on, error, &notice);
                message = error.empty() ? mod.info.name + (on ? " enabled." : " disabled.") : error;
                if (!notice.empty()) message += " " + notice;
                refresh = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine(); ImGui::TextDisabled("%s", mod.info.version.c_str());
            ImGui::Indent();
            ImGui::TextWrapped("%s", mod.info.description.c_str());
            if (!live) ImGui::TextDisabled("Restart required: change this mod from the launcher.");
            if (!mod.problem.empty()) ImGui::TextWrapped("Unavailable: %s", mod.problem.c_str());
            for (const auto &warning : mod.warnings) ImGui::TextWrapped("%s", warning.c_str());
            if (mod_overlay::has_settings(mod.info.id) && ImGui::TreeNode("Options")) {
                mod_overlay::draw_settings(mod.info.id);
                ImGui::TreePop();
            }
            ImGui::Unindent(); ImGui::Spacing(); ImGui::Separator();
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::TextWrapped("%s", message.empty() ? "Changes are saved automatically." : message.c_str());
        if (first_frame) ImGui::SetKeyboardFocusHere();
        if (ImGui::Button("Close")) open = false;
        ImGui::SameLine(); if (ImGui::Button("Refresh")) refresh = true;
        const auto cancel = ui::Layer::get().confirm_south() ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown;
        if (ui::Layer::get().gamepad_armed() && ImGui::IsKeyPressed(cancel, false)) open = false;
    }
    ImGui::End();
    if (refresh) list = mods::installed();
    return open;
}
namespace {
std::string size_text(std::uint64_t bytes) {
    char text[32];
    if (bytes >= 1024ull * 1024 * 1024) std::snprintf(text, sizeof text, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024ull * 1024) std::snprintf(text, sizeof text, "%.1f MB", bytes / (1024.0 * 1024));
    else if (bytes >= 1024) std::snprintf(text, sizeof text, "%.1f KB", bytes / 1024.0);
    else std::snprintf(text, sizeof text, "%llu B", static_cast<unsigned long long>(bytes));
    return text;
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
    enum class Stage { List, Catalog, Fetching, Downloading, Review, AlreadyInstalled, ConfirmDowngrade, Installing, Result };
    Stage stage = Stage::List;
    std::optional<mods::ModPreview> preview;
    auto progress = std::make_shared<mods::InstallProgress>();
    std::future<std::pair<bool, std::string>> job;
    std::future<catalog::Listing> catalog_job;
    std::future<catalog::Download> download_job;
    auto transfer = std::make_shared<catalog::Progress>();
    struct CatalogGroup {
        catalog::Source source;
        catalog::Listing listing;
        bool expanded{}, loaded{};
    };
    std::string index_error;
    const auto index_file = install::user_data_directory() / "mod-indexes.txt";
    std::vector<CatalogGroup> groups;
    for (const auto &source : catalog::load_sources(index_file, index_error)) groups.push_back({source});
    std::size_t active_group{};
    std::optional<catalog::Entry> selected;
    bool catalog_review = false;
    const auto download_cache = install::user_data_directory() / "mod-downloads";
    std::string message, error, notice;
    bool result_ok = false;
    std::vector<mods::InstalledMod> list = mods::installed();
    bool done = false, refocus = true;
    bool texture_dump = texture_dump_setting();
    std::string dump_error;
    const auto go = [&](Stage next) { stage = next; refocus = true; };
    const auto review = [&] {
        const auto &p = *preview;
        go(p.ok() && p.replaces && !p.info.version.empty() && p.info.version == p.installed_version
            ? Stage::AlreadyInstalled : Stage::Review);
    };
    const auto begin_install = [&] {
        progress = std::make_shared<mods::InstallProgress>();
        job = std::async(std::launch::async, [p = *preview, progress] {
            std::string why;
            const bool ok = mods::install(p, *progress, why);
            return std::make_pair(ok, why);
        });
        go(Stage::Installing);
    };

    const auto refresh_group = [&](std::size_t which) {
        active_group = which;
        const auto source = groups[which].source;
        const auto cache = catalog::cache_directory(download_cache, source);
        transfer = std::make_shared<catalog::Progress>();
        catalog_job = std::async(std::launch::async, [source, cache, transfer] {
            return catalog::fetch(cache, *transfer, source);
        });
        go(Stage::Fetching);
    };
    const auto persist_groups = [&](const std::vector<CatalogGroup> &updated) {
        std::vector<catalog::Source> sources;
        for (const auto &group : updated) sources.push_back(group.source);
        catalog::save_sources(index_file, sources);
    };
    const bool kept = layer.run([&] {
        if (text_input_open()) {
            text_input_frame();
            return true;
        }
        const auto cancel = layer.confirm_south() ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown;
        const bool back = layer.take_back() || ImGui::IsKeyPressed(cancel, false);
        // A file dropped on the window opens the install pop-up for it.
        if (auto dropped = layer.take_dropped_file(); dropped && stage != Stage::Installing && stage != Stage::Fetching && stage != Stage::Downloading) {
            catalog_review = false;
            preview = mods::inspect(*dropped);
            review();
        }
        if (stage == Stage::Installing && job.valid() &&
            job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto [ok, why] = job.get();
            if (ok) mods::restore_overlays();
            result_ok = ok;
            message = ok ? ui::host_text::format("launcher.mods.installed_message", {preview->info.name})
                         : ui::host_text::format("launcher.mods.not_installed_message", {why});
            list = mods::installed();
            for (const auto &mod : list)
                if (ok && mod.enabled && mod.info.id == preview->info.id) {
                    layer.renderer().reload_texture_pack();
                    message = ui::host_text::format("launcher.mods.updated_message", {preview->info.name});
                }
            go(Stage::Result);
        }

        if (stage == Stage::Fetching && catalog_job.valid() &&
            catalog_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            groups[active_group].listing = catalog_job.get();
            groups[active_group].loaded = true;
            go(Stage::Catalog);
        }
        if (stage == Stage::Downloading && download_job.valid() &&
            download_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = download_job.get();
            if (result.file.empty()) {
                groups[active_group].listing.error = result.error;
                go(Stage::Catalog);
            } else {
                preview = mods::inspect(result.file);
                if (!preview->info.has_manifest || preview->info.id != selected->id || preview->info.version != selected->version)
                    preview->problem = ui::host_text::get("launcher.catalog.identity_mismatch");
                catalog_review = true;
                review();
            }
        }

        begin_panel("##fu_mods", ui::host_text::cstr("launcher.mods.mods"), game().project_name, false);
        begin_content();
        const bool focus = refocus;
        refocus = false;
        switch (stage) {
        case Stage::List: {
            if (focus) focus_next_row();
            if (button_row(ui::host_text::cstr("launcher.catalog.browse"))) go(Stage::Catalog);
            section(ui::host_text::cstr("launcher.mods.install_a_mod"));
            paragraph(ui::host_text::cstr("launcher.mods.to_install_a_mod_drag"));
            paragraph(ui::host_text::cstr("launcher.mods.installed_mods_are_listed_below"),
                      colors::kTextDim);
            if (list.empty()) {
                section(ui::host_text::cstr("launcher.mods.installed_mods"));
                paragraph(ui::host_text::cstr("launcher.mods.no_mods_installed_yet"), colors::kTextDim);
            }
            bool first_row = false;
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

            }
            if (changed) {
                list = mods::installed();
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
        case Stage::Catalog: {
            heading(ui::host_text::get("launcher.catalog.title"));
            paragraph(ui::host_text::get("launcher.catalog.index_help"), colors::kTextDim);
            if (!index_error.empty()) paragraph(index_error, colors::kDanger);
            if (focus) focus_next_row();
            if (button_row(ui::host_text::cstr("launcher.catalog.add_index"))) {
                TextInputRequest request;
                request.title = ui::host_text::get("launcher.catalog.add_index");
                request.prompt = ui::host_text::get("launcher.catalog.index_address_help");
                request.max_length = 2048;
                open_text_input(std::move(request), [&](std::optional<std::string> text) {
                    if (!text) return;
                    try {
                        const auto source = catalog::source_from_url(*text);
                        for (const auto &group : groups) if (group.source.url == source.url)
                            throw std::runtime_error(ui::host_text::get("launcher.catalog.duplicate_index"));
                        auto updated = groups;
                        updated.push_back({source, {}, true});
                        persist_groups(updated);
                        groups = std::move(updated);
                        index_error.clear();
                        refresh_group(groups.size() - 1);
                    } catch (const std::exception &e) { index_error = e.what(); }
                });
            }
            for (std::size_t g = 0; g < groups.size(); ++g) {
                auto &group = groups[g];
                auto &catalogue = group.listing;
                const std::string label = std::string(group.expanded ? "-  " : "+  ") +
                    group.source.name + "##index-" + group.source.url;
                const auto detail = group.loaded ? ui::host_text::format("launcher.catalog.mod_count", {std::to_string(catalogue.index.mods.size())}) :
                    ui::host_text::get("launcher.catalog.open_index");
                if (value_row(label.c_str(), detail)) {
                    group.expanded = !group.expanded;
                    if (group.expanded && !group.loaded) { refresh_group(g); break; }
                }
                if (!group.expanded) continue;
                ImGui::Indent(ImGui::GetFontSize());
                paragraph(group.source.url, colors::kTextDim);
                if (catalogue.cached) paragraph(ui::host_text::get("launcher.catalog.cached"), colors::kTextDim);
                if (!catalogue.error.empty()) paragraph(catalogue.error, colors::kDanger);
                if (button_row((ui::host_text::get("launcher.catalog.refresh_index") + "##refresh-" + group.source.url).c_str())) {
                    refresh_group(g);
                    ImGui::Unindent(ImGui::GetFontSize());
                    break;
                }
                if (g != 0 && button_row((ui::host_text::get("launcher.catalog.remove_index") + "##remove-" + group.source.url).c_str())) {
                    try {
                        auto updated = groups;
                        updated.erase(updated.begin() + g);
                        persist_groups(updated);
                        groups = std::move(updated);
                        index_error.clear();
                    } catch (const std::exception &e) { index_error = e.what(); }
                    ImGui::Unindent(ImGui::GetFontSize());
                    break;
                }
                if (catalogue.index.mods.empty()) paragraph(ui::host_text::get("launcher.catalog.empty"), colors::kTextDim);
                for (const auto &entry : catalogue.index.mods) {
                    RowOptions options;
                    for (const auto &installed : list) if (installed.info.id == entry.id) {
                        options.note = ui::host_text::format("launcher.catalog.installed", {installed.info.version});
                        break;
                    }
                    if (options.note.empty()) options.note = size_text(entry.bytes);
                    options.description = ui::host_text::get("launcher.catalog.review_help");
                    const auto entry_detail = options.note;
                    options.note.clear();
                    if (value_row((entry.name + "  v" + entry.version + "##catalog-" + group.source.url + "-" + entry.id).c_str(), entry_detail, options)) {
                        selected = entry;
                        active_group = g;
                        catalogue.error.clear();
                        bool already_installed = false;
                        for (const auto &installed : list)
                            if (installed.info.id == entry.id && installed.info.version == entry.version)
                                already_installed = true;
                        if (already_installed) {
                            preview = mods::ModPreview{};
                            preview->info.name = entry.name;
                            catalog_review = true;
                            go(Stage::AlreadyInstalled);
                            break;
                        }
                        const auto cache = catalog::cache_directory(download_cache, group.source);
                        transfer = std::make_shared<catalog::Progress>();
                        download_job = std::async(std::launch::async, [entry, cache, transfer] {
                            return catalog::download(entry, cache, *transfer);
                        });
                        go(Stage::Downloading);
                        break;
                    }
                }
                ImGui::Unindent(ImGui::GetFontSize());
                if (stage != Stage::Catalog) break;
            }
            if (button_row(ui::host_text::cstr("launcher.mods.back"))) go(Stage::List);
            break;
        }
        case Stage::Fetching:
        case Stage::Downloading: {
            heading(stage == Stage::Fetching ? ui::host_text::get("launcher.catalog.fetching") :
                    ui::host_text::format("launcher.catalog.downloading", {selected->name}));
            const auto total = transfer->total.load();
            progress_bar(total ? static_cast<float>(transfer->bytes.load()) / total : 0.0f,
                         ui::host_text::format("launcher.mods.progress", {size_text(transfer->bytes.load()), size_text(total)}));
            if (transfer->cancel) paragraph(ui::host_text::get("launcher.catalog.cancelling"), colors::kTextDim);
            else if (button_row(ui::host_text::cstr("launcher.mods.cancel"))) transfer->cancel = true;
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
                    if (p.replaces && !p.installed_version.empty() && !p.info.version.empty() &&
                        version_is_newer(p.installed_version, p.info.version)) go(Stage::ConfirmDowngrade);
                    else begin_install();
                }
                if (button_row(ui::host_text::cstr("launcher.mods.cancel"))) go(catalog_review ? Stage::Catalog : Stage::List);
            } else {
                paragraph(p.problem);
                if (focus) focus_next_row();
                if (button_row(ui::host_text::cstr("launcher.mods.ok"))) go(Stage::List);
            }
            break;
        }
        case Stage::AlreadyInstalled:
            heading(preview->info.name);
            paragraph(ui::host_text::cstr("launcher.mods.already_installed"));
            if (focus) focus_next_row();
            if (button_row(ui::host_text::cstr("launcher.mods.ok"))) go(catalog_review ? Stage::Catalog : Stage::List);
            break;
        case Stage::ConfirmDowngrade:
            heading(preview->info.name);
            paragraph(ui::host_text::cstr("launcher.mods.confirm_downgrade"));
            info_row(ui::host_text::cstr("launcher.mods.version"), preview->info.version);
            paragraph(ui::host_text::format("launcher.mods.replaces_version", {preview->installed_version}), colors::kTextDim);
            if (button_row(ui::host_text::cstr("dialog.button.yes"))) begin_install();
            if (focus) focus_next_row();
            if (button_row(ui::host_text::cstr("dialog.button.no"))) go(catalog_review ? Stage::Catalog : Stage::List);
            break;
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
            else if (stage == Stage::Fetching || stage == Stage::Downloading) transfer->cancel = true;
            else if ((stage == Stage::Review || stage == Stage::AlreadyInstalled || stage == Stage::ConfirmDowngrade) && catalog_review) go(Stage::Catalog);
            else if (stage != Stage::Installing) go(Stage::List);
        }
        return !done;
    }, false);
    if (text_input_open()) cancel_text_input();
    transfer->cancel = true;
    if (catalog_job.valid()) catalog_job.wait();
    if (download_job.valid()) download_job.wait();
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
namespace portablekit::fu { bool show_mods_page() { return true; } bool game_mods_frame(bool) { return false; } }
#endif
