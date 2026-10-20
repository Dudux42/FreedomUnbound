#include "gathering_nodes.hpp"
#include "gathering_marker_layout.hpp"
#include "damage_events.hpp"
#include "mod_overlay.hpp"
#include "launcher_ini.hpp"
#include "imgui.h"
#include "ui/layer.hpp"
#include "gpu/vulkan_renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <charconv>
#include <string>
namespace portablekit::fu::gathering_nodes {
namespace {
struct Filter {Kind kind;const char *label;const char *key;};
constexpr Filter filters[]{
    {Kind::Mine,"Mining","gathering_nodes.mine"},
    {Kind::Bugs,"Bug catching","gathering_nodes.bugs"},
    {Kind::Gather,"Regular gathering","gathering_nodes.gather"},
    {Kind::Carve,"Carving","gathering_nodes.carve"}};
unsigned icon_mask=0x1e;
float icon_size=1.f;
bool size_dirty{};
std::string options_error;
unsigned bit(Kind kind){return 1u<<static_cast<unsigned>(kind);}
void load_options() {
    static const bool loaded=[] {
        for(const auto &filter:filters)if(launcher_ini::get(filter.key)=="0")icon_mask &= ~bit(filter.kind);
        if(const auto text=launcher_ini::get("gathering_nodes.size")) {
            float parsed{};const auto result=std::from_chars(text->data(),text->data()+text->size(),parsed);
            if(result.ec==std::errc{} && result.ptr==text->data()+text->size() && std::isfinite(parsed))icon_size=std::clamp(parsed,.75f,2.f);
        }
        return true;
    }();(void)loaded;
}
void save_size() {
    if(!size_dirty)return;
    options_error=launcher_ini::set("gathering_nodes.size",std::to_string(icon_size))?"":"Could not save icon size.";
    size_dirty=false;
}
void settings() {
    load_options();ImGui::TextUnformatted("Show icons for:");
    for(const auto &filter:filters) {
        bool on=(icon_mask&bit(filter.kind))!=0;
        if(ImGui::Checkbox(filter.label,&on)) {
            if(launcher_ini::set(filter.key,on?"1":"0")) {
                if(on)icon_mask |= bit(filter.kind);else icon_mask &= ~bit(filter.kind);
                options_error.clear();
            }else options_error="Could not save this icon toggle.";
        }
    }
    if(ImGui::SliderFloat("Icon size",&icon_size,.75f,2.f,"%.2fx"))size_dirty=true;
    if(!ImGui::IsAnyItemActive())save_size();
    ImGui::TextDisabled("Hiding an icon does not change gathering controls.");
    if(!options_error.empty())ImGui::TextWrapped("%s",options_error.c_str());
}
bool shown(Status status) {
    return status.kind>=Kind::Gather && status.kind<=Kind::Carve && (icon_mask&bit(status.kind))!=0;
}
bool fresh(Status status) {
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return status.kind != Kind::None && now >= status.timestamp && now - status.timestamp < 250000000;
}
void enable(const std::filesystem::path &) { load_options();set_enabled(true); damage_events::set_camera_enabled(true); }
void disable() { save_size();set_enabled(false); damage_events::set_camera_enabled(false); }
bool visible() {
    if(size_dirty && !ImGui::IsAnyItemActive())save_size();
    if (ui::Layer::get().attached()) ui::Layer::get().renderer().set_overlay_projection_enabled(true);
    const auto status=snapshot();return fresh(status) && shown(status);
}
void draw() {
    const auto status = snapshot();
    const auto camera = damage_events::camera();
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!fresh(status) || !shown(status) || !camera.valid || now - static_cast<double>(camera.monotonic_ns) / 1e9 >= .25) return;
    const auto point = ui::Layer::get().renderer().project_overlay_world(status.head, &camera.view);
    if (!point) return;
    const auto *vp = ImGui::GetMainViewport();
    if (std::getenv("MHFU_GATHERING_PREVIEW"))
        ImGui::GetForegroundDrawList()->AddText({vp->Pos.x + 16, vp->Pos.y + vp->Size.y - 36},
            IM_COL32(255, 220, 140, 255), "Gathering preview - sample marker above hunter");
    const float r = std::clamp(vp->Size.y * (25.0f / 544.0f), 16.0f, 64.0f) * icon_size;
    const auto position = marker_position((*point)[0] * vp->Size.x, (*point)[1] * vp->Size.y,
                                          vp->Size.x, vp->Size.y, r);
    const ImVec2 center{vp->Pos.x + position[0], vp->Pos.y + position[1]};
    auto *list = ImGui::GetForegroundDrawList();
    // Forest-green hexagonal badge matching the Freedom Unbound emblem.
    ImVec2 hex[6];
    for (int i = 0; i < 6; ++i) {
        const float angle = (-90.0f + i * 60.0f) * 3.14159265f / 180.0f;
        hex[i] = {center.x + std::cos(angle) * r, center.y + std::sin(angle) * r};
    }
    list->AddConvexPolyFilled(hex, 6, IM_COL32(16, 96, 46, 245));
    list->AddPolyline(hex, 6, IM_COL32(0, 0, 0, 255), ImDrawFlags_Closed, r * .12f);
    list->AddPolyline(hex, 6, IM_COL32(82, 155, 94, 255), ImDrawFlags_Closed, r * .035f);
    for (int i = 0; i < 6; ++i) list->AddLine(center, hex[i], IM_COL32(26, 116, 55, 220), r * .045f);
    const auto p = [&](float x, float y) { return ImVec2{center.x + x*r, center.y + y*r}; };
    const auto ink = IM_COL32(255, 255, 240, 255);
    const auto edge = IM_COL32(0, 0, 0, 255);
    const auto line = [&](ImVec2 a, ImVec2 b, ImU32 color, float width) {
        list->AddLine(a,b,edge,width+r*.09f);
        list->AddLine(a,b,color,width);
    };
    if (status.kind == Kind::Mine) {
        line(p(-.43f,.62f),p(.27f,-.43f),IM_COL32(218,170,104,255),r*.17f);
        list->AddBezierQuadratic(p(-.65f,-.24f),p(.12f,-.9f),p(.67f,.02f),edge,r*.27f);
        list->AddBezierQuadratic(p(-.65f,-.24f),p(.12f,-.9f),p(.67f,.02f),ink,r*.17f);
    } else if (status.kind == Kind::Bugs) {
        line(p(-.46f,.65f),p(.18f,-.17f),IM_COL32(218,170,104,255),r*.14f);
        // Open hoop and mesh make a net rather than a butterfly silhouette.
        const auto hoop=p(.15f,-.28f);
        list->AddCircle(hoop,r*.4f,edge,24,r*.17f);
        list->AddCircle(hoop,r*.4f,ink,24,r*.08f);
        for (float offset : {-.15f,0.f,.15f}) {
            list->AddLine(p(-.16f,-.28f+offset),p(.46f,-.28f+offset),IM_COL32(204,233,220,255),r*.035f);
            list->AddLine(p(.15f+offset,-.59f),p(.15f+offset,.03f),IM_COL32(204,233,220,255),r*.035f);
        }
    } else if (status.kind == Kind::Carve) {
        list->AddTriangleFilled(p(-.16f,.22f),p(.52f,-.67f),p(.43f,.02f),edge);
        list->AddTriangleFilled(p(-.1f,.16f),p(.48f,-.57f),p(.36f,.0f),ink);
        line(p(-.47f,.59f),p(-.13f,.19f),IM_COL32(218,170,104,255),r*.19f);
        line(p(-.27f,.05f),p(.02f,.32f),ink,r*.09f);
    } else {
        auto *font = ImGui::GetFont();
        const float size = r * 1.65f;
        const auto extent = font->CalcTextSizeA(size, 10000, 0, "?");
        const ImVec2 at{center.x - extent.x * .5f, center.y - extent.y * .5f - r * .05f};
        for (const ImVec2 d : {ImVec2{-2, 0}, ImVec2{2, 0}, ImVec2{0, -2}, ImVec2{0, 2}})
            list->AddText(font, size, {at.x + d.x, at.y + d.y}, IM_COL32(0, 0, 0, 255), "?");
        for (float dx : {-r * .025f, 0.0f, r * .025f})
            list->AddText(font, size, {at.x + dx, at.y}, IM_COL32(255, 255, 240, 255), "?");
    }

}
}
void register_renderer() {
    static const bool registered = mod_overlay::register_provider(
        {"gathering-nodes", {"gathering_nodes", "gathering_repeat"}, enable, disable, visible, draw, false, settings});
    (void)registered;
}
}
