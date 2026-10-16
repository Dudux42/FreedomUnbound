#include "modern_camera.hpp"
#include "monster_catalog.hpp"
#include "target_icon.hpp"
#include "profile.hpp"
#include "mod_overlay.hpp"
#include "psprecomp/runtime.hpp"
#include "settings/settings.hpp"
#include <cstdlib>
#include <algorithm>
namespace portablekit::fu::modern_camera {
namespace {
void frame(psprecomp::Runtime &r) {
    const auto &s = settings::current();
    prepare(r.memory().aot_fast_view(), s.analog_camera && s.right_stick != settings::RightStick::Off);
}
void enable(const std::filesystem::path &) { set_enabled(true); }
void disable() { set_enabled(false); }
bool visible() { return !mod_overlay::is_active("monster-hp") && target().identity != 0; }
void draw() {
    if (mod_overlay::is_active("monster-hp")) return;
    const auto selected = target();
    const auto *name = monster_hp::large_monster_name(selected.species);
    if (!selected.identity || !name) return;
    const auto *vp = ImGui::GetMainViewport();
    const float scale = std::clamp(vp->Size.y / 544.0f, .65f, 3.0f);
    auto *list = ImGui::GetForegroundDrawList();
    auto *font = ImGui::GetFont();
    const float width = font->CalcTextSizeA(14 * scale, 10000, 0, name).x + 42 * scale;
    const float x = vp->Pos.x + 18 * scale, y = vp->Pos.y + vp->Size.y - 48 * scale;
    list->AddRectFilled({x,y}, {x+width,y+30*scale}, IM_COL32(12,12,16,220), 5*scale);
    draw_target_icon(list, {x+15*scale,y+15*scale}, 8*scale);
    list->AddText(font, 14*scale, {x+31*scale,y+7*scale}, IM_COL32(245,245,245,255), name);
    if (const auto *flag = std::getenv("MHFU_TARGET_CAMERA_PREVIEW"); flag && flag[0] == '1' && !flag[1])
        list->AddText(font, 10*scale, {x,y-14*scale}, IM_COL32(255,224,112,255), "Target preview - sample monsters");
}
}
const CameraDriver driver{.frame = frame, .driving = driving, .triggers = triggers};
void register_mod() {
    static const bool registered = [] {
        const bool ok = mod_overlay::register_provider({"modern-camera", {"modern_camera", "target_camera"}, enable, disable, visible, draw});
        const auto *preview = std::getenv("MHFU_MODERN_CAMERA_TEST");
        if (ok && preview && preview[0] == '1' && !preview[1]) set_enabled(true);
        return ok;
    }();
    (void)registered;
}
}
