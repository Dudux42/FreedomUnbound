#include "cheats.hpp"
#include "install/user_data.hpp"
#include <fstream>
#include <string>

#if defined(PORTABLEKIT_HAS_RENDERER)
#include "profile.hpp"
#include "ui/layer.hpp"
#include "ui/widgets.hpp"
#include "ui/host_text.hpp"
#include "imgui.h"
#endif

namespace portablekit::fu {
namespace {
bool loaded{};
bool save_failed{};
void save_preferences() {
    std::ofstream file(install::user_data_directory() / "cheats.ini", std::ios::trunc);
    file << "health=" << !!(cheats() & Health) << '\n'
         << "stamina=" << !!(cheats() & Stamina) << '\n'
         << "one_hit=" << !!(cheats() & OneHit) << '\n'
         << "money=" << !!(cheats() & Money) << '\n'
         << "box_stacks=" << !!(cheats() & BoxStacks) << '\n'
         << "free_craft=" << !!(cheats() & FreeCraft) << '\n'
         << "quest_complete=" << !!(cheats() & QuestComplete) << '\n';
    file.flush();
    save_failed = !file;
}
}
void load_cheat_preferences() {
    if (loaded) return;
    loaded = true;
    std::uint32_t mask{};
    std::ifstream file(install::user_data_directory() / "cheats.ini");
    std::string line;
    while (std::getline(file, line)) {
        if (line == "health=1") mask |= Health;
        if (line == "stamina=1") mask |= Stamina;
        if (line == "one_hit=1") mask |= OneHit;
        if (line == "money=1") mask |= Money;
        if (line == "box_stacks=1") mask |= BoxStacks;
        if (line == "free_craft=1") mask |= FreeCraft;
        if (line == "quest_complete=1") mask |= QuestComplete;
    }
    set_cheats(mask);
}
void draw_cheat_rows() {
    load_cheat_preferences();
#if defined(PORTABLEKIT_HAS_RENDERER)
    using namespace ui;
    section(host_text::cstr("launcher.cheats.progression_testing"));
    paragraph(host_text::cstr("launcher.cheats.intro"));
    const auto row = [](const char *label, Cheat bit, const char *description) {
        if (toggle_row(label, (cheats() & bit) != 0u, {false, {}, description})) {
            set_cheats(cheats() ^ bit);
            save_preferences();
        }
    };
    row(host_text::cstr("launcher.cheats.unlimited_health"), Health, host_text::cstr("launcher.cheats.unlimited_health_help"));
    row(host_text::cstr("launcher.cheats.unlimited_stamina"), Stamina, host_text::cstr("launcher.cheats.unlimited_stamina_help"));
    row(host_text::cstr("launcher.cheats.one_hit_kills"), OneHit, host_text::cstr("launcher.cheats.one_hit_kills_help"));
    row(host_text::cstr("launcher.cheats.max_money"), Money, host_text::cstr("launcher.cheats.max_money_help"));
    row(host_text::cstr("launcher.cheats.box_stacks"), BoxStacks, host_text::cstr("launcher.cheats.box_stacks_help"));
    row(host_text::cstr("launcher.cheats.free_craft"), FreeCraft, host_text::cstr("launcher.cheats.free_craft_help"));
    row(host_text::cstr("launcher.cheats.quest_complete"), QuestComplete, host_text::cstr("launcher.cheats.quest_complete_help"));
    if (button_row(host_text::cstr("launcher.cheats.disable_all"))) { set_cheats(0); save_preferences(); }
    if (crafting_restart_required()) paragraph(host_text::cstr("launcher.cheats.restart_required"));
    if (save_failed) paragraph(host_text::cstr("launcher.cheats.save_failed"));
#endif
}
bool show_cheats_page() {
#if defined(PORTABLEKIT_HAS_RENDERER)
    using namespace ui;
    auto &layer = Layer::get();
    bool done = false, first = true;
    return layer.run([&] {
        const auto cancel = layer.confirm_south() ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown;
        if (layer.take_back() || ImGui::IsKeyPressed(cancel, false)) done = true;
        begin_panel("##fu_cheats", host_text::cstr("launcher.cheats.title"), game().project_name, false);
        begin_content();
        if (first) focus_next_row();
        first = false;
        draw_cheat_rows();
        if (button_row(host_text::cstr("launcher.cheats.back"))) done = true;
        begin_footer();
        hints({{Control::Confirm, host_text::cstr("launcher.cheats.select")}, {Control::Back, host_text::cstr("launcher.cheats.back")}});
        end_panel();
        return !done;
    }, false);
#else
    return true;
#endif
}
}
