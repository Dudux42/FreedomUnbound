#include "buff_icons.hpp"
#include "mod_overlay.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../portablekit/third_party/stb_image.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace portablekit::fu::buff_icons {
namespace {
std::filesystem::path mod_folder;
std::array<ImTextureData *,capacity> icons{};
void load_icon(Kind kind,const char *name) {
    std::ifstream file(mod_folder/"icons"/name,std::ios::binary);
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
    icons[static_cast<unsigned>(kind)]=texture;
}
void enable(const std::filesystem::path &folder) {
    mod_folder=folder;
    load_icon(Kind::Attack,"AttackUp02.png");
    load_icon(Kind::Recovery,"DmgRecUp.png");
    load_icon(Kind::Heat,"HotDrink.png");
    load_icon(Kind::Cold,"CoolDrink.png");
    set_enabled(true);std::puts("[buff-icons] enabled");
}
void disable() {set_enabled(false);}
bool visible() {return snapshot().count!=0;}
void glyph(ImDrawList *d,ImVec2 c,float r,Kind kind,ImU32 ink) {
    const float w=std::max(1.f,r*.18f);
    auto p=[&](float x,float y){return ImVec2{c.x+x*r,c.y+y*r};};
    auto line=[&](float x,float y,float a,float b){d->AddLine(p(x,y),p(a,b),ink,w);};
    switch(kind) {
    case Kind::Attack:
        line(-.5f,.5f,.38f,-.38f);line(-.4f,.05f,-.05f,.4f);
        d->AddTriangleFilled(p(.2f,-.44f),p(.48f,-.48f),p(.44f,-.2f),ink);break;
    case Kind::Defense: {
        const ImVec2 points[]{p(-.43f,-.4f),p(.43f,-.4f),p(.35f,.18f),p(0,.52f),p(-.35f,.18f)};
        d->AddPolyline(points,5,ink,ImDrawFlags_Closed,w);break;
    }
    case Kind::Stamina:
        d->AddTriangleFilled(p(.12f,-.58f),p(-.45f,.1f),p(.12f,.1f),ink);
        d->AddTriangleFilled(p(-.12f,.58f),p(.45f,-.1f),p(-.12f,-.1f),ink);break;
    case Kind::Recovery:line(-.47f,0,.47f,0);line(0,-.47f,0,.47f);break;
    case Kind::Heat:
        d->AddTriangleFilled(p(0,-.58f),p(-.43f,.35f),p(.43f,.35f),ink);
        d->AddCircleFilled(p(0,.22f),r*.38f,ink,12);break;
    case Kind::Cold:
        for(unsigned i=0;i<3;i++) {
            const float angle=i*3.14159265f/3, x=std::cos(angle)*.57f,y=std::sin(angle)*.57f;
            line(-x,-y,x,y);
        }break;
    case Kind::Vision:
        d->AddEllipse(c,{r*.62f,r*.35f},ink,0,16,w);d->AddCircleFilled(c,r*.15f,ink,10);break;
    default:
        line(.18f,-.5f,.18f,.32f);line(.18f,-.5f,.48f,-.35f);
        d->AddEllipse(p(-.02f,.35f),{r*.26f,r*.18f},ink,-.3f,12,w);break;
    }
}
void draw() {
    const auto data=snapshot();if(!data.count)return;
    const auto *vp=ImGui::GetMainViewport();
    // Guest HUD coordinates: first icon immediately after the sharpness blade.
    // Use the same stretched framebuffer mapping as the native HUD at any aspect.
    const float sx=vp->Size.x/480.f,sy=vp->Size.y/272.f;
    const float scale=std::min(sx,sy),radius=5.f*scale;
    const float pitch=std::min(14.f*sx,(vp->Size.x-120*sx)/std::max(1u,data.count-1));
    auto *d=ImGui::GetForegroundDrawList();
    constexpr ImU32 colors[]{IM_COL32(245,90,77,255),IM_COL32(105,157,255,255),
        IM_COL32(250,220,75,255),IM_COL32(143,222,103,255),IM_COL32(255,150,67,255),
        IM_COL32(118,222,242,255),IM_COL32(200,127,235,255)};
    for(unsigned i=0;i<data.count;i++) {
        const auto &buff=data.buffs[i];const auto a=appearance(buff.frames,ImGui::GetTime());
        const ImVec2 center{vp->Pos.x+110*sx+i*pitch,vp->Pos.y+37*sy};
        const float r=radius*a.scale;
        const auto index=static_cast<unsigned>(buff.kind);
        ImU32 base;
        Kind shape=buff.kind;
        if(index<item_count)base=colors[index];
        else {
            const auto melody=index-item_count;
            float red,green,blue;
            ImGui::ColorConvertHSVtoRGB(std::fmod(melody*.61803398875f,.999f),.48f,1.f,red,green,blue);
            base=ImGui::ColorConvertFloat4ToU32({red,green,blue,1});
            // Shared-effect melodies retain the familiar effect silhouette.
            switch(melody) {
            case 1:shape=Kind::Attack;break;case 2:shape=Kind::Defense;break;
            case 5:shape=Kind::Stamina;break;case 7:shape=Kind::Recovery;break;
            case 9:shape=Kind::Heat;break;case 10:shape=Kind::Cold;break;
            default:break;
            }
        }
        const auto ink=a.red ? IM_COL32(255,48,48,255) : base;
        if(index<item_count && icons[index] && icons[index]->GetTexID()!=ImTextureID_Invalid) {
            auto *texture=icons[index];
            const ImVec2 extent{r*2.2f*texture->Width/float(std::max(texture->Width,texture->Height)),
                                r*2.2f*texture->Height/float(std::max(texture->Width,texture->Height))};
            const ImVec2 at{center.x-extent.x*.5f,center.y-extent.y*.5f};
            d->AddImage(texture->GetTexRef(),at,{at.x+extent.x,at.y+extent.y});
            if(a.red)d->AddCircle(center,r+scale,IM_COL32(255,48,48,255),20,std::max(1.f,scale));
        } else {
            d->AddCircleFilled(center,r+scale,IM_COL32(5,12,13,215),20);
            d->AddCircle(center,r,ink,20,std::max(1.f,scale));
            glyph(d,center,r,shape,ink);
        }
        if(index>=item_count && shape!=buff.kind) {
            const ImVec2 note{center.x+r*.72f,center.y+r*.65f};
            d->AddCircleFilled(note,r*.32f,IM_COL32(5,12,13,245),10);
            d->AddLine({note.x,note.y-r*.25f},{note.x,note.y+r*.1f},ink,std::max(1.f,scale*.6f));
            d->AddCircleFilled({note.x-r*.09f,note.y+r*.12f},r*.13f,ink,8);
        }
    }
}
}
void register_renderer() {
    static const bool registered=mod_overlay::register_provider({"buff-icons",{"buff_icons"},enable,disable,visible,draw});
    (void)registered;
}
}
