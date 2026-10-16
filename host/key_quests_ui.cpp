#include "key_quests.hpp"
#include "mod_overlay.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace portablekit::fu::key_quests {
namespace {
void enable(const std::filesystem::path &) {
    set_enabled(true);
    std::puts("[key-quests] enabled"); std::fflush(stdout);
}
void disable() { set_enabled(false); }
void draw() {
    const auto data=take_markers();
    const auto *vp=ImGui::GetMainViewport();
    auto *list=ImGui::GetForegroundDrawList();
    const float sx=vp->Size.x/480.f, sy=vp->Size.y/272.f;
    list->PushClipRect(vp->Pos,{vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y},true);
    for(std::size_t i=0;i<data.count;i++) {
        const auto &marker=data.markers[i];
        ImVec2 points[10];
        for(int j=0;j<10;j++) {
            const float angle=(-90.f+j*36.f)*3.14159265f/180.f;
            const float radius=j%2 ? 2.f : 4.5f;
            points[j]={vp->Pos.x+(marker.x+std::cos(angle)*radius)*sx,
                       vp->Pos.y+(marker.y+std::sin(angle)*radius)*sy};
        }
        list->AddConcavePolyFilled(points,10,IM_COL32(255,209,83,255));
        list->AddPolyline(points,10,IM_COL32(55,35,12,255),ImDrawFlags_Closed,std::max(.75f,sy*.65f));
    }
    list->PopClipRect();
}
}
void register_renderer() {
    static const bool registered=mod_overlay::register_provider(
        {"key-quests",{"key_quests"},enable,disable,visible,draw});
    (void)registered;
}
}
