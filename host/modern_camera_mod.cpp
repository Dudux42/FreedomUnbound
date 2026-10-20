#include "modern_camera.hpp"
#include "monster_catalog.hpp"
#include "target_icon.hpp"
#include "profile.hpp"
#include "mod_overlay.hpp"
#include "psprecomp/runtime.hpp"
#include "settings/settings.hpp"
#include "launcher_ini.hpp"
#include "imgui.h"
#include <cstdlib>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
namespace portablekit::fu::modern_camera {
namespace {
Controls options;
bool options_loaded{}, options_dirty{};
std::string save_error;
void load_options() {
    if(options_loaded)return;
    options_loaded=true;
    const auto number=[](const char *key,float fallback,float low,float high) {
        const auto value=launcher_ini::get(key);if(!value)return fallback;
        float parsed{};const auto result=std::from_chars(value->data(),value->data()+value->size(),parsed);
        return result.ec==std::errc{} && result.ptr==value->data()+value->size() && std::isfinite(parsed)
            ?std::clamp(parsed,low,high):fallback;
    };
    options.horizontal_sensitivity=number("modern_camera.horizontal_sensitivity",1.f,.25f,2.f);
    options.vertical_sensitivity=number("modern_camera.vertical_sensitivity",1.f,.25f,2.f);
    options.smoothing=number("modern_camera.smoothing",0.f,0.f,100.f);
    options.invert_horizontal=launcher_ini::get("modern_camera.invert_horizontal")=="1";
    options.invert_vertical=launcher_ini::get("modern_camera.invert_vertical")=="1";
    options.targeting=launcher_ini::get("modern_camera.targeting")!="0";
    set_controls(options);
}
void save_options() {
    if(!options_dirty)return;
    bool ok=launcher_ini::set("modern_camera.horizontal_sensitivity",std::to_string(options.horizontal_sensitivity));
    ok=launcher_ini::set("modern_camera.vertical_sensitivity",std::to_string(options.vertical_sensitivity))&&ok;
    ok=launcher_ini::set("modern_camera.smoothing",std::to_string(options.smoothing))&&ok;
    ok=launcher_ini::set("modern_camera.invert_horizontal",options.invert_horizontal?"1":"0")&&ok;
    ok=launcher_ini::set("modern_camera.invert_vertical",options.invert_vertical?"1":"0")&&ok;
    ok=launcher_ini::set("modern_camera.targeting",options.targeting?"1":"0")&&ok;
    save_error=ok?"":"Could not save all camera options. Changes apply for this session.";options_dirty=false;
}
void settings() {
    load_options();
    const bool changed=ImGui::SliderFloat("Horizontal sensitivity",&options.horizontal_sensitivity,.25f,2.f,"%.2fx")|
        ImGui::SliderFloat("Vertical sensitivity",&options.vertical_sensitivity,.25f,2.f,"%.2fx")|
        ImGui::Checkbox("Invert horizontal axis",&options.invert_horizontal)|
        ImGui::Checkbox("Invert vertical axis",&options.invert_vertical)|
        ImGui::SliderFloat("Smoothing strength",&options.smoothing,0.f,100.f,"%.0f%%")|
        ImGui::Checkbox("Monster targeting",&options.targeting);
    if(changed){options_dirty=true;set_controls(options);}
    ImGui::TextDisabled("Sensitivity and axis options apply to the quest camera. Targeting controls ZL/ZR.");
    if(ImGui::Button("Restore defaults")){options=Controls{};options_dirty=true;set_controls(options);}
    if(!ImGui::IsAnyItemActive())save_options();
    if(!save_error.empty()){ImGui::TextWrapped("%s",save_error.c_str());if(ImGui::Button("Retry saving"))options_dirty=true;}
}
void frame(psprecomp::Runtime &r) {
    const auto &s = settings::current();
    prepare(r.memory().aot_fast_view(), s.analog_camera && s.right_stick != settings::RightStick::Off);
}
void enable(const std::filesystem::path &) { load_options();set_enabled(true); }
void disable() { save_options();set_enabled(false); }
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
        const bool ok = mod_overlay::register_provider({"modern-camera", {"modern_camera", "target_camera"}, enable, disable, visible, draw, false, settings});
        const auto *preview = std::getenv("MHFU_MODERN_CAMERA_TEST");
        if (ok && preview && preview[0] == '1' && !preview[1]) set_enabled(true);
        return ok;
    }();
    (void)registered;
}
}
