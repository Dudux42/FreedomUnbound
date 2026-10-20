#include "return_timer.hpp"
#include "mod_overlay.hpp"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
namespace portablekit::fu::return_timer {
namespace {
void enable(const std::filesystem::path &) { set_enabled(true);std::puts("[return-timer] enabled");std::fflush(stdout); }
void disable() { set_enabled(false); }
bool visible() { return snapshot().frames!=0; }
void draw() {
    const auto data=snapshot();if(!data.frames)return;
    const auto *vp=ImGui::GetMainViewport();
    const float scale=std::clamp(vp->Size.y/544.f,.65f,3.f);
    const float width=150*scale,height=56*scale;
    const ImVec2 at{vp->Pos.x+(vp->Size.x-width)*.5f,vp->Pos.y+80*scale};
    auto *list=ImGui::GetForegroundDrawList();auto *font=ImGui::GetFont();
    const auto ink=data.seconds<=10 ? IM_COL32(255,112,84,255) : IM_COL32(255,238,199,255);
    list->AddRectFilled(at,{at.x+width,at.y+height},IM_COL32(14,19,18,220),7*scale);
    list->AddRect(at,{at.x+width,at.y+height},IM_COL32(191,159,90,210),7*scale,0,scale);
    const char *label="RETURN IN";
    const auto label_size=font->CalcTextSizeA(11*scale,10000,0,label);
    list->AddText(font,11*scale,{at.x+(width-label_size.x)*.5f,at.y+6*scale},IM_COL32(220,202,159,255),label);
    char time[24];std::snprintf(time,sizeof(time),"%02u:%02u",data.seconds/60,data.seconds%60);
    const auto extent=font->CalcTextSizeA(28*scale,10000,0,time);
    list->AddText(font,28*scale,{at.x+(width-extent.x)*.5f,at.y+20*scale},ink,time);
}
}
void register_renderer() {
    static const bool registered=mod_overlay::register_provider({"return-timer",{"return_timer"},enable,disable,visible,draw});
    (void)registered;
}
}
