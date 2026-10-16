#include "monster_hp.hpp"
#include "modern_camera.hpp"
#include "target_icon.hpp"
#include "monster_catalog.hpp"
#include "mod_overlay.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace portablekit::fu::monster_hp {
namespace {
std::array<std::uint32_t, 8> identities{};
std::array<unsigned, 8> maxima{};
std::array<float, 8> displayed{};
bool preview{};
Snapshot read_status() {
    if (!preview) return snapshot();
    // Explicit labeled layout fixture, never enabled by the desktop shortcut.
    Snapshot data;
    data.count = 2;
    data.monsters[0] = {1, 1500, 6000, 0, 1, 1};
    data.monsters[1] = {2, 2800, 4000, 0, 11, 1};
    return data;
}
void enable(const std::filesystem::path &) { set_enabled(true); identities = {}; maxima = {}; }
void disable() { set_enabled(false); identities = {}; maxima = {}; }
bool visible() { return read_status().count != 0; }
void draw() {
    const auto data = read_status();
    if (!data.count) return;
    const auto *vp = ImGui::GetMainViewport();
    const float scale = std::clamp(vp->Size.y / 544.0f, .65f, 3.0f);
    const float width = std::min(310.0f * scale, vp->Size.x * .40f);
    const float x = vp->Pos.x + 18 * scale;
    float y = vp->Pos.y + vp->Size.y - 18 * scale;
    auto *list = ImGui::GetForegroundDrawList();
    auto *font = ImGui::GetFont();
    const auto selected = modern_camera::target();
    for (std::size_t i = 0; i < data.count; ++i) {
        const auto &monster = data.monsters[i];
        const float fraction = static_cast<float>(monster.hp) / monster.maximum;
        if (identities[i] != monster.identity || maxima[i] != monster.maximum) {
            identities[i] = monster.identity; maxima[i] = monster.maximum; displayed[i] = fraction;
        } else {
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, .1f);
            displayed[i] += (fraction - displayed[i]) * (1.0f - std::exp(-12.0f * dt));
        }
        const float height = 52.0f * scale;
        y -= height;
        list->AddRectFilled({x, y}, {x + width, y + height}, IM_COL32(12, 12, 16, 220), 5 * scale);
        const float bar_y = y + 24 * scale;
        const float bar_h = 11.0f * scale;
        list->AddRectFilled({x + 8 * scale, bar_y}, {x + width - 8 * scale, bar_y + bar_h}, IM_COL32(36, 36, 42, 255), 2 * scale);
        const auto color = fraction > .5f ? IM_COL32(224, 82, 82, 255) :
            fraction > .25f ? IM_COL32(224, 168, 82, 255) : IM_COL32(82, 200, 120, 255);
        if (displayed[i] > .001f)
            list->AddRectFilled({x + 8 * scale, bar_y}, {x + 8 * scale + (width - 16 * scale) * displayed[i], bar_y + bar_h}, color, 2 * scale);
        if (selected.identity == monster.identity && selected.species == monster.species)
            draw_target_icon(list, {x + width - 16 * scale, bar_y + bar_h * .5f}, 4.5f * scale);
        char label[96];
        const auto *name = large_monster_name(monster.species);
        if (!name) continue;
        if (monster.hp) std::snprintf(label, sizeof(label), "%s   %u / %u   %.0f%%", name, monster.hp, monster.maximum, fraction * 100);
        else std::snprintf(label, sizeof(label), "%s   DEAD", name);
        const float measured = font->CalcTextSizeA(14 * scale, 10000, 0, label).x;
        const float text_size = 14 * scale * std::min(1.0f, (width - 16 * scale) / std::max(1.0f, measured));
        list->AddText(font, text_size, {x + 8 * scale, y + 5 * scale}, IM_COL32(245, 245, 245, 255), label);
        if (low_hp(monster))
            list->AddText(font, 10 * scale, {x + 8 * scale, y + 38 * scale}, IM_COL32(240, 205, 100, 255), "LOW HP (25% or less)");
        y -= 5 * scale;
    }
    for (auto i = data.count; i < identities.size(); ++i) identities[i] = 0;
    if (preview)
        list->AddText(font, 12 * scale, {x, y - 16 * scale}, IM_COL32(255, 220, 140, 255), "HP preview - sample values");
}
}
void register_renderer() {
    static const bool registered = mod_overlay::register_provider(
        {"monster-hp", {"monster_hp"}, enable, disable, visible, draw, true});
    (void)registered;
    static const bool initialized = [] {
        const char *flag = std::getenv("MHFU_MONSTER_HP_PREVIEW");
        if (flag && flag[0] == '1' && flag[1] == '\0') {
            preview = true;
            mod_overlay::activate("monster-hp", {});
        }
        return true;
    }();
    (void)initialized;
}
}
