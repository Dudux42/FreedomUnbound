#include "monster_hp.hpp"
#include "modern_camera.hpp"
#include "target_icon.hpp"
#include "monster_catalog.hpp"
#include "mod_overlay.hpp"
#include "launcher_ini.hpp"
#include "equipment_wishlist_model.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <charconv>
#include <string>
namespace portablekit::fu::monster_hp {
namespace {
std::array<std::uint32_t, 8> identities{};
std::array<unsigned, 8> maxima{};
std::array<float, 8> displayed{};
bool preview{};
struct Options {float size=1.f;int display=2;bool custom{};float color[3]{224/255.f,82/255.f,82/255.f};};
Options options;
bool dirty{};
std::string save_error;
wishlist::Notifications notifications;
void load_options() {
    static const bool loaded=[] {
        if(const auto value=launcher_ini::get("monster_hp.size")) {
            float size{};const auto r=std::from_chars(value->data(),value->data()+value->size(),size);
            if(r.ec==std::errc{} && r.ptr==value->data()+value->size() && std::isfinite(size))options.size=std::clamp(size,.75f,2.f);
        }
        if(const auto value=launcher_ini::get("monster_hp.display")) {
            int display{};const auto r=std::from_chars(value->data(),value->data()+value->size(),display);
            if(r.ec==std::errc{} && r.ptr==value->data()+value->size() && display>=0 && display<=2)options.display=display;
        }
        options.custom=launcher_ini::get("monster_hp.custom_color")=="1";
        if(const auto value=launcher_ini::get("monster_hp.color");value && value->size()==6) {
            unsigned rgb{};const auto r=std::from_chars(value->data(),value->data()+6,rgb,16);
            if(r.ec==std::errc{} && r.ptr==value->data()+6)for(unsigned i=0;i<3;i++)options.color[i]=float((rgb>>(16-i*8))&255)/255.f;
        }
        return true;
    }();(void)loaded;
}
void save_options() {
    if(!dirty)return;
    char color[7];std::snprintf(color,sizeof(color),"%02X%02X%02X",int(std::lround(options.color[0]*255)),int(std::lround(options.color[1]*255)),int(std::lround(options.color[2]*255)));
    bool ok=launcher_ini::set("monster_hp.size",std::to_string(options.size));
    ok=launcher_ini::set("monster_hp.display",std::to_string(options.display)) && ok;
    ok=launcher_ini::set("monster_hp.custom_color",options.custom?"1":"0") && ok;
    ok=launcher_ini::set("monster_hp.color",color) && ok;
    save_error=ok?"":"Could not save all options. Changes apply for this session.";dirty=false;
}
void settings() {
    load_options();dirty=ImGui::SliderFloat("Bar size",&options.size,.75f,2.f,"%.2fx") || dirty;
    constexpr const char *formats[]{"Current / maximum HP","Percentage","Both"};
    dirty=ImGui::Combo("HP display",&options.display,formats,3) || dirty;
    dirty=ImGui::Checkbox("Use custom bar color",&options.custom) || dirty;
    if(ImGui::ColorEdit3("Bar color",options.color,ImGuiColorEditFlags_NoInputs)){options.custom=true;dirty=true;}
    ImGui::TextDisabled("Without a custom color, the bar changes color with health.");
    if(ImGui::Button("Restore defaults")){options=Options{};dirty=true;}
    if(!ImGui::IsAnyItemActive())save_options();
    if(!save_error.empty()){ImGui::TextWrapped("%s",save_error.c_str());if(ImGui::Button("Retry saving"))dirty=true;}
}
Snapshot read_status() {
    if (!preview) return snapshot();
    // Explicit labeled layout fixture, never enabled by the desktop shortcut.
    Snapshot data;
    data.count = 2;
    data.quest = true;
    data.monsters[0] = {1, 1500, 6000, 0, 1, 1};
    data.monsters[1] = {2, 2800, 4000, 0, 11, 1};
    return data;
}
void enable(const std::filesystem::path &) { load_options();set_enabled(true); identities = {}; maxima = {};notifications.clear(); }
void disable() { save_options();set_enabled(false); identities = {}; maxima = {};notifications.clear(); }
bool visible() {
    if(dirty && !ImGui::IsAnyItemActive())save_options();
    const auto data=read_status();const auto events=take_breaks();
    if(!data.quest){notifications.clear();return false;}
    for(const auto &event:events)if(const auto *name=large_monster_name(event.species))
        notifications.push({false,0,event.count,name});
    return data.count || notifications.queued() || !notifications.cards().empty();
}
void draw_notifications(float bar_top) {
    notifications.advance(ImGui::GetIO().DeltaTime);
    const auto *vp=ImGui::GetMainViewport();auto *list=ImGui::GetForegroundDrawList();auto *font=ImGui::GetFont();
    const float scale=std::clamp(vp->Size.y/544.f,.65f,2.f);
    const float width=std::min(210*scale,vp->Size.x*.4f),height=38*scale,gap=5*scale;
    const float anchor=std::max(vp->Pos.y+2*(height+gap)+8*scale,
        std::min(vp->Pos.y+vp->Size.y*.5f,bar_top-height-gap));
    for(const auto &card:notifications.cards()) {
        const auto motion=wishlist::motion(card.age);if(motion.alpha<=0)continue;
        const int alpha=int(255*motion.alpha);
        const ImVec2 at{vp->Pos.x+14*scale,anchor+card.y*(height+gap)+motion.offset*scale};
        list->AddRectFilled(at,{at.x+width,at.y+height},IM_COL32(12,21,19,int(225*motion.alpha)),5*scale);
        list->AddRectFilled({at.x,at.y+5*scale},{at.x+3*scale,at.y+height-5*scale},IM_COL32(245,205,100,alpha));
        const char *title=card.notice.quantity>1?"Parts destroyed":"Part destroyed";
        list->AddText(font,11*scale,{at.x+10*scale,at.y+5*scale},IM_COL32(245,205,100,alpha),title);
        const float size=12*scale*std::min(1.f,(width-20*scale)/std::max(1.f,font->CalcTextSizeA(12*scale,10000,0,card.notice.equipment.c_str()).x));
        list->AddText(font,size,{at.x+10*scale,at.y+20*scale},IM_COL32(245,245,240,alpha),card.notice.equipment.c_str());
    }
}
void draw() {
    const auto data = read_status();
    if(!data.quest){notifications.clear();return;}
    const auto *vp = ImGui::GetMainViewport();
    if (!data.count) {draw_notifications(vp->Pos.y+vp->Size.y);return;}
    // Keep every current-area monster on screen even at a large chosen scale.
    const float scale = std::min(std::clamp(vp->Size.y / 544.0f, .65f, 3.0f)*options.size,
        std::max(.1f,(vp->Size.y-24.f)/(data.count*89.f+18.f)));
    const float width = std::min(310.0f * scale, vp->Size.x * .40f);
    const float x = vp->Pos.x + 18 * scale;
    float y = vp->Pos.y + vp->Size.y - 18 * scale;
    draw_notifications(y-data.count*89*scale+5*scale);
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
        const float height = 84.0f * scale;
        y -= height;
        list->AddRectFilled({x, y}, {x + width, y + height}, IM_COL32(12, 12, 16, 220), 5 * scale);
        const float bar_y = y + 24 * scale;
        const float bar_h = 11.0f * scale;
        list->AddRectFilled({x + 8 * scale, bar_y}, {x + width - 8 * scale, bar_y + bar_h}, IM_COL32(36, 36, 42, 255), 2 * scale);
        const auto color = options.custom ? ImGui::ColorConvertFloat4ToU32({options.color[0],options.color[1],options.color[2],1.f}) : fraction > .5f ? IM_COL32(224, 82, 82, 255) :
            fraction > .25f ? IM_COL32(224, 168, 82, 255) : IM_COL32(82, 200, 120, 255);
        if (displayed[i] > .001f)
            list->AddRectFilled({x + 8 * scale, bar_y}, {x + 8 * scale + (width - 16 * scale) * displayed[i], bar_y + bar_h}, color, 2 * scale);
        if (selected.identity == monster.identity && selected.species == monster.species)
            draw_target_icon(list, {x + width - 16 * scale, bar_y + bar_h * .5f}, 4.5f * scale);
        char label[96];
        const auto *name = large_monster_name(monster.species);
        if (!name) continue;
        if (monster.hp && options.display==0)std::snprintf(label,sizeof(label),"%s   %u / %u",name,monster.hp,monster.maximum);
        else if(monster.hp && options.display==1)std::snprintf(label,sizeof(label),"%s   %.0f%%",name,fraction*100);
        else if (monster.hp) std::snprintf(label, sizeof(label), "%s   %u / %u   %.0f%%", name, monster.hp, monster.maximum, fraction * 100);
        else std::snprintf(label, sizeof(label), "%s   DEAD", name);
        const float measured = font->CalcTextSizeA(14 * scale, 10000, 0, label).x;
        const float text_size = 14 * scale * std::min(1.0f, (width - 16 * scale) / std::max(1.0f, measured));
        list->AddText(font, text_size, {x + 8 * scale, y + 5 * scale}, IM_COL32(245, 245, 245, 255), label);
        if(monster.hp) {
            constexpr const char *names[]{"Poison","Paralysis","Sleep","Stun"};
            constexpr ImU32 colors[]{IM_COL32(191,130,237,255),IM_COL32(245,205,95,255),IM_COL32(115,185,245,255),IM_COL32(245,160,90,255)};
            const float cell=(width-31*scale)/4;
            for(unsigned n=0;n<4;n++) {
                const auto &status=monster.status[n];const float left=x+8*scale+n*(cell+5*scale);
                const float name_size=10*scale*std::min(1.f,cell/std::max(1.f,font->CalcTextSizeA(10*scale,10000,0,names[n]).x));
                list->AddText(font,name_size,{left,y+39*scale},colors[n],names[n]);
                char value[32];if(status.valid)std::snprintf(value,sizeof(value),"%u / %u",status.current,status.threshold);else std::snprintf(value,sizeof(value),"N/A");
                const float value_size=9*scale*std::min(1.f,cell/std::max(1.f,font->CalcTextSizeA(9*scale,10000,0,value).x));
                list->AddText(font,value_size,{left,y+51*scale},IM_COL32(225,225,230,255),value);
                const ImVec2 at{left,y+64*scale};list->AddRectFilled(at,{left+cell,at.y+3*scale},IM_COL32(45,45,52,255));
                if(status.valid)list->AddRectFilled(at,{left+cell*std::clamp(float(status.current)/status.threshold,0.f,1.f),at.y+3*scale},colors[n]);
            }
            if(low_hp(monster))list->AddText(font,9*scale,{x+8*scale,y+71*scale},IM_COL32(240,205,100,255),"LOW HP (25% or less)");
        }
        y -= 5 * scale;
    }
    for (auto i = data.count; i < identities.size(); ++i) identities[i] = 0;
    if (preview)
        list->AddText(font, 12 * scale, {x, y - 16 * scale}, IM_COL32(255, 220, 140, 255), "HP preview - sample values");
}
}
void register_renderer() {
    static const bool registered = mod_overlay::register_provider(
        {"monster-hp", {"monster_hp"}, enable, disable, visible, draw, true, settings});
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
