#include "gathering_map.hpp"
#include "mod_overlay.hpp"
#include "launcher_ini.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../portablekit/third_party/stb_image.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
namespace portablekit::fu::gathering_map {
namespace {
std::array<ImTextureData *,5> icons{};
void load_icon(unsigned index,const std::filesystem::path &folder,const char *name) {
    if(index>=icons.size() || icons[index])return;
    std::ifstream file(folder/"icons"/name,std::ios::binary);
    if(!file)return;
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.empty() || bytes.size()>1024*1024)return;
    int width=0,height=0,channels=0;
    auto *pixels=stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),&width,&height,&channels,4);
    if(!pixels)return;
    auto *texture=new ImTextureData;
    texture->Create(ImTextureFormat_RGBA32,width,height);
    std::memcpy(texture->GetPixels(),pixels,static_cast<size_t>(width)*height*4);
    stbi_image_free(pixels);
    ImGui::RegisterUserTexture(texture);
    icons[index]=texture;
}
constexpr const char *keys[]{"gathering_map.gather", "gathering_map.mine", "gathering_map.bugs", "gathering_map.carve", "gathering_map.fishing"};
void load_options() {
    static const bool loaded=[] {
        unsigned mask=31;
        for(unsigned i=0;i<5;++i)if(launcher_ini::get(keys[i])=="0")mask &= ~(1u<<i);
        set_filters(mask);return true;
    }();
    (void)loaded;
}
void options() {
    load_options();
    static std::string error;
    constexpr const char *labels[]{"Regular gathering", "Mining", "Bug catching", "Carving", "Fishing"};
    for(unsigned i : {1u,2u,4u,0u,3u}) {
        const auto mask=filters();bool on=(mask & (1u<<i))!=0;
        if(ImGui::Checkbox(labels[i],&on)) {
            if(launcher_ini::set(keys[i],on?"1":"0")) {
                set_filters(on?mask|(1u<<i):mask&~(1u<<i));error.clear();
            } else error="Could not save this option.";
        }
    }
    if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
}
void enable(const std::filesystem::path &folder) {
    load_icon(static_cast<unsigned>(Kind::Mine),folder,"pickaxe.png");
    load_icon(static_cast<unsigned>(Kind::Bugs),folder,"bug-net.png");
    load_icon(static_cast<unsigned>(Kind::Gather),folder,"herb.png");
    load_icon(static_cast<unsigned>(Kind::Fishing),folder,"fishing-rod.png");
    load_options();set_enabled(true);
}
void disable() {set_enabled(false);}
bool fresh(const View &v) {
    const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return v.timestamp && now>=v.timestamp && now-v.timestamp<150000000;
}
bool visible() {const auto v=snapshot();return fresh(v) && !v.markers.empty();}
void draw() {
    const auto v=snapshot();if(!fresh(v))return;
    const auto *vp=ImGui::GetMainViewport();auto *d=ImGui::GetForegroundDrawList();
    const float sx=vp->Size.x/480,sy=vp->Size.y/272,r=std::clamp(std::min(sx,sy)*2.4f,2.4f,12.f);
    const auto point=[&](float x,float y){return ImVec2{vp->Pos.x+x*sx,vp->Pos.y+y*sy};};
    d->PushClipRect(point(v.left,v.top),point(v.right,v.bottom),true);
    for(auto marker:v.markers) {
        const auto p=point(marker.x,marker.y);
        const auto index=static_cast<unsigned>(marker.kind);
        const auto color=marker.kind==Kind::Mine ? IM_COL32(123,203,255,245) : marker.kind==Kind::Bugs ? IM_COL32(255,216,101,245) : marker.kind==Kind::Carve ? IM_COL32(245,236,220,245) : marker.kind==Kind::Fishing ? IM_COL32(108,226,221,245) : IM_COL32(125,230,143,245);
        d->AddCircleFilled(p,r+1.3f,IM_COL32(7,18,14,225));
        if(index<icons.size() && icons[index] && icons[index]->GetTexID()!=ImTextureID_Invalid) {
            auto *texture=icons[index];
            const float extent=r*2.35f;
            const ImVec2 at{p.x-extent*.5f,p.y-extent*.5f};
            d->AddImage(texture->GetTexRef(),at,{at.x+extent,at.y+extent});
        } else if(marker.kind==Kind::Mine) {
            d->AddLine({p.x-r*.6f,p.y+r*.7f},{p.x+r*.4f,p.y-r*.65f},color,r*.5f);
            d->AddBezierQuadratic({p.x-r,p.y-r*.35f},{p.x,p.y-r*1.2f},{p.x+r,p.y+r*.15f},color,r*.5f);
        } else if(marker.kind==Kind::Bugs) {
            d->AddCircleFilled({p.x-r*.45f,p.y-r*.2f},r*.55f,color);
            d->AddCircleFilled({p.x+r*.45f,p.y-r*.2f},r*.55f,color);
            d->AddLine({p.x,p.y-r*.6f},{p.x,p.y+r*.75f},IM_COL32(255,249,218,255),r*.45f);
        } else if(marker.kind==Kind::Carve) {
            d->AddTriangleFilled({p.x-r*.5f,p.y+r*.2f},{p.x+r*.85f,p.y-r*.85f},{p.x+r*.35f,p.y+r*.15f},color);
            d->AddLine({p.x-r*.8f,p.y+r*.8f},{p.x-r*.25f,p.y+r*.25f},IM_COL32(240,141,105,255),r*.45f);
        } else if(marker.kind==Kind::Fishing) {
            const ImVec2 fish[]{
                {p.x-r,p.y},{p.x-r*.42f,p.y-r*.62f},{p.x+r*.35f,p.y-r*.5f},
                {p.x+r*.15f,p.y},{p.x+r*.35f,p.y+r*.5f},{p.x-r*.42f,p.y+r*.62f}
            };
            d->AddConvexPolyFilled(fish,6,color);
            d->AddPolyline(fish,6,IM_COL32(7,18,14,255),ImDrawFlags_Closed,std::max(1.f,r*.18f));
            d->AddTriangleFilled({p.x+r*.2f,p.y},{p.x+r*.95f,p.y-r*.6f},{p.x+r*.95f,p.y+r*.6f},color);
            d->AddCircleFilled({p.x-r*.48f,p.y-r*.08f},std::max(1.f,r*.13f),IM_COL32(7,18,14,255));
        } else {
            const ImVec2 leaf[]{
                {p.x-r*.9f,p.y+r*.2f},{p.x-r*.48f,p.y-r*.5f},{p.x+r*.15f,p.y-r*.82f},
                {p.x+r*.88f,p.y-r*.28f},{p.x+r*.42f,p.y+r*.52f},{p.x-r*.24f,p.y+r*.75f}
            };
            d->AddConvexPolyFilled(leaf,6,color);
            d->AddPolyline(leaf,6,IM_COL32(7,18,14,255),ImDrawFlags_Closed,std::max(.8f,r*.14f));
            d->AddLine({p.x-r*.72f,p.y+r*.46f},{p.x+r*.67f,p.y-r*.44f},IM_COL32(220,255,222,255),std::max(.8f,r*.14f));
        }
    }
    d->PopClipRect();
}
}
void register_renderer() {
    static const bool registered=mod_overlay::register_provider({"gathering-map-markers",{"gathering_map_markers"},enable,disable,visible,draw,false,options});
    (void)registered;
}
}
