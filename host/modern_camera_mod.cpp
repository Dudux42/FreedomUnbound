#include "modern_camera.hpp"
#include "camera_aim.hpp"
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
    options.response_curve=number("modern_camera.response_curve",1.8f,1.f,3.f);
    options.pitch_min=number("modern_camera.pitch_min",-20.f,-45.f,0.f);
    options.pitch_max=number("modern_camera.pitch_max",70.f,30.f,85.f);
    options.smoothing=number("modern_camera.smoothing",0.f,0.f,100.f);
    options.invert_horizontal=launcher_ini::get("modern_camera.invert_horizontal")=="1";
    options.invert_vertical=launcher_ini::get("modern_camera.invert_vertical")=="1";
    options.targeting=launcher_ini::get("modern_camera.targeting")!="0";
    options.aim_sensitivity=number("modern_camera.aim_sensitivity",1.f,.25f,2.f);
    options.aim_curve=number("modern_camera.aim_curve",2.f,1.f,3.f);
    options.aim_smoothing=number("modern_camera.aim_smoothing",30.f,0.f,100.f);
    options.aim_deadzone=number("modern_camera.aim_deadzone",.12f,.05f,.4f);
    options.aim_enabled=launcher_ini::get("modern_camera.aim_enabled")!="0";
    options.aim_invert_horizontal=launcher_ini::get("modern_camera.aim_invert_horizontal")=="1";
    options.aim_invert_vertical=launcher_ini::get("modern_camera.aim_invert_vertical")=="1";
    set_controls(options);
}
void save_options() {
    if(!options_dirty)return;
    bool ok=launcher_ini::set("modern_camera.horizontal_sensitivity",std::to_string(options.horizontal_sensitivity));
    ok=launcher_ini::set("modern_camera.vertical_sensitivity",std::to_string(options.vertical_sensitivity))&&ok;
    ok=launcher_ini::set("modern_camera.response_curve",std::to_string(options.response_curve))&&ok;
    ok=launcher_ini::set("modern_camera.pitch_min",std::to_string(options.pitch_min))&&ok;
    ok=launcher_ini::set("modern_camera.pitch_max",std::to_string(options.pitch_max))&&ok;
    ok=launcher_ini::set("modern_camera.smoothing",std::to_string(options.smoothing))&&ok;
    ok=launcher_ini::set("modern_camera.invert_horizontal",options.invert_horizontal?"1":"0")&&ok;
    ok=launcher_ini::set("modern_camera.invert_vertical",options.invert_vertical?"1":"0")&&ok;
    ok=launcher_ini::set("modern_camera.targeting",options.targeting?"1":"0")&&ok;
    ok=launcher_ini::set("modern_camera.aim_sensitivity",std::to_string(options.aim_sensitivity))&&ok;
    ok=launcher_ini::set("modern_camera.aim_curve",std::to_string(options.aim_curve))&&ok;
    ok=launcher_ini::set("modern_camera.aim_smoothing",std::to_string(options.aim_smoothing))&&ok;
    ok=launcher_ini::set("modern_camera.aim_deadzone",std::to_string(options.aim_deadzone))&&ok;
    ok=launcher_ini::set("modern_camera.aim_enabled",options.aim_enabled?"1":"0")&&ok;
    ok=launcher_ini::set("modern_camera.aim_invert_horizontal",options.aim_invert_horizontal?"1":"0")&&ok;
    ok=launcher_ini::set("modern_camera.aim_invert_vertical",options.aim_invert_vertical?"1":"0")&&ok;
    save_error=ok?"":"Could not save all camera options. Changes apply for this session.";options_dirty=false;
}
void settings() {
    load_options();
    bool changed=ImGui::SliderFloat("Horizontal sensitivity",&options.horizontal_sensitivity,.25f,2.f,"%.2fx")|
        ImGui::SliderFloat("Vertical sensitivity",&options.vertical_sensitivity,.25f,2.f,"%.2fx")|
        ImGui::Checkbox("Invert horizontal axis",&options.invert_horizontal)|
        ImGui::Checkbox("Invert vertical axis",&options.invert_vertical)|
        ImGui::SliderFloat("Smoothing strength",&options.smoothing,0.f,100.f,"%.0f%%")|
        ImGui::SliderFloat("Stick response curve",&options.response_curve,1.f,3.f,"%.2f")|
        ImGui::SliderFloat("Minimum pitch",&options.pitch_min,-45.f,0.f,"%.0f degrees")|
        ImGui::SliderFloat("Maximum pitch",&options.pitch_max,30.f,85.f,"%.0f degrees")|
        ImGui::Checkbox("Monster targeting",&options.targeting);
    if(mod_overlay::has_hook("modern-camera","camera_aim") && camera_aim::armed()) {
        ImGui::Separator();ImGui::TextUnformatted("Ranged aiming");
        changed|=ImGui::Checkbox("Right-stick aiming",&options.aim_enabled);
        changed|=ImGui::SliderFloat("Aim sensitivity",&options.aim_sensitivity,.25f,2.f,"%.2fx");
        changed|=ImGui::SliderFloat("Aim response curve",&options.aim_curve,1.f,3.f,"%.2f");
        changed|=ImGui::SliderFloat("Aim smoothing",&options.aim_smoothing,0.f,100.f,"%.0f%%");
        changed|=ImGui::SliderFloat("Aim dead zone",&options.aim_deadzone,.05f,.4f,"%.2f");
        changed|=ImGui::Checkbox("Invert aim horizontally",&options.aim_invert_horizontal);
        changed|=ImGui::Checkbox("Invert aim vertically",&options.aim_invert_vertical);
    }
    if(changed){options_dirty=true;set_controls(options);}
    ImGui::TextDisabled("Sensitivity and axis options apply to the quest camera. Targeting controls ZL/ZR.");
    if(ImGui::Button("Restore defaults")){options=Controls{};options_dirty=true;set_controls(options);}
    if(!ImGui::IsAnyItemActive())save_options();
    if(!save_error.empty()){ImGui::TextWrapped("%s",save_error.c_str());if(ImGui::Button("Retry saving"))options_dirty=true;}
}
void frame(psprecomp::Runtime &r) {
    const auto &s = settings::current();
    const auto &player=s;
    configure_aim_input(s.aim_speed,player.right_dead_zone<0?player.dead_zone:player.right_dead_zone,
        player.invert_camera_x,player.invert_camera_y);
    prepare(r.memory().aot_fast_view(), s.analog_camera && s.right_stick != settings::RightStick::Off);
}
void enable(const std::filesystem::path &) {
    load_options();set_enabled(true);camera_aim::reset();
    camera_aim::set_provider(game_camera_aim);camera_aim::set_enabled(true);
}
void disable() { save_options();camera_aim::set_enabled(false);camera_aim::set_provider(nullptr);set_enabled(false); }
bool aim_boost() { return camera_aim::aiming(); }
float degrees_per_second() {
    const auto &s=settings::current();
    return camera_aim::aiming()?s.aim_speed*controls().aim_sensitivity:s.camera_speed;
}
bool mouse_aim(float &,float &) {return false;}
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
const CameraDriver driver{.frame=frame,.driving=driving,.aim_boost=aim_boost,.mouse_aim=mouse_aim,
    .degrees_per_second=degrees_per_second,.triggers=triggers};
void register_mod() {
    static const bool registered = [] {
        const bool ok = mod_overlay::register_provider({"modern-camera", {"modern_camera", "target_camera", "camera_aim"}, enable, disable, visible, draw, false, settings});
        const auto *preview = std::getenv("MHFU_MODERN_CAMERA_TEST");
        if (ok && preview && preview[0] == '1' && !preview[1]) set_enabled(true);
        return ok;
    }();
    (void)registered;
}
}
