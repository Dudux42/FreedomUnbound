#include "controller_prompts.hpp"
#include "native_prompts.hpp"
#include "equipment_wishlist.hpp"
#include "modern_camera.hpp"
#include "mod_overlay.hpp"
#include "ui/layer.hpp"
#include "gpu/vulkan_renderer.hpp"
#include "settings/settings.hpp"
#include "imgui.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace portablekit::fu::controller_prompts {
namespace {
bool enabled{};
Family family() {
    auto &layer=ui::Layer::get();
    auto *pad=layer.attached() ? layer.renderer().gamepad() : nullptr;
    if(!pad || layer.input_device()==ui::InputDevice::Keyboard)return Family::Keyboard;
    switch(SDL_GetGamepadType(pad)) {
    case SDL_GAMEPAD_TYPE_PS3:case SDL_GAMEPAD_TYPE_PS4:case SDL_GAMEPAD_TYPE_PS5:return Family::PlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:return Family::Nintendo;
    case SDL_GAMEPAD_TYPE_XBOX360:case SDL_GAMEPAD_TYPE_XBOXONE:return Family::Xbox;
    default:return Family::Generic;
    }
}
void left(float x,float y,float size){draw(Action::ExtraLeft,x,y,size);}
void right(float x,float y,float size){draw(Action::ExtraRight,x,y,size);}
float width(Action action) {
    return std::max(22.f,ImGui::GetFont()->CalcTextSizeA(11,10000,0,current_label(action).c_str()).x+10);
}
wishlist::ControlHints hints() {
    return {{current_label(Action::ExtraRight),right,width(Action::ExtraRight)},
            {current_label(Action::ExtraLeft),left,width(Action::ExtraLeft)}};
}
void enable(const std::filesystem::path &) {enabled=true;enable_native(true);wishlist::set_control_hint_provider(hints);}
void disable(){enabled=false;enable_native(false);wishlist::set_control_hint_provider(nullptr);}
bool visible(){return enabled && modern_camera::target().identity!=0;}
void render() {
    const auto *vp=ImGui::GetMainViewport();
    const float scale=std::min(vp->Size.x/480.f,vp->Size.y/272.f);
    const float size=12*scale;
    // Two short rows above the native minimap leave the item-name strip clear.
    const float w=std::max(width(Action::ExtraLeft),width(Action::ExtraRight))*12.f/14.f*scale+68*scale;
    const float x=vp->Pos.x+vp->Size.x-w-5*scale;
    const float y=vp->Pos.y+2*scale;
    auto *d=ImGui::GetForegroundDrawList();
    d->AddRectFilled({x-4*scale,y-scale},{vp->Pos.x+vp->Size.x-2*scale,y+29*scale},IM_COL32(12,21,19,215),3*scale);
    const auto first=draw(Action::ExtraLeft,x,y,size)+4*scale;
    d->AddText(ImGui::GetFont(),10*scale,{x+first,y+scale},IM_COL32(240,240,230,255),"Face target");
    const auto second=draw(Action::ExtraRight,x,y+15*scale,size)+4*scale;
    d->AddText(ImGui::GetFont(),10*scale,{x+second,y+16*scale},IM_COL32(240,240,230,255),"Next target");
}
}
std::string current_label(Action action) {
    const auto index=static_cast<std::size_t>(action==Action::ExtraLeft ? input::Action::ExtraLeft : input::Action::ExtraRight);
    auto result=label(family(),action,input::format(settings::current().bindings[index]));
    if(const auto *trace=std::getenv("MHFU_CONTROLLER_PROMPTS_TRACE");trace && trace[0]=='1') {
        static std::string previous[2];const unsigned slot=action==Action::ExtraRight;
        if(previous[slot]!=result){previous[slot]=result;std::printf("[controller-prompts] %s=%s\n",slot ? "right" : "left",result.c_str());std::fflush(stdout);}
    }
    return result;
}
Family current_family() {return family();}
std::string mapped_single_label(input::Action action) {
    const auto &cfg=settings::current();const auto f=family();
    if(f==Family::Keyboard) {
        const auto &slots=cfg.bindings[static_cast<std::size_t>(action)];
        for(auto binding:slots)if(binding!=input::kNone)return input::name(binding);
        return "--";
    }
    auto button=input::kPadNone;
    for(auto binding:cfg.pad_bindings[static_cast<std::size_t>(action)])
        if(binding!=input::kPadNone){button=binding;break;}
    if(button==input::kPadConfirm)button=cfg.confirm_south ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
    if(button==input::kPadCancel)button=cfg.confirm_south ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH;
    if(button<=SDL_GAMEPAD_BUTTON_NORTH) {
        static const char *xbox[]{"A","B","X","Y"};
        static const char *nintendo[]{"B","A","Y","X"};
        static const char *ps[]{"Cross","Circle","Square","Triangle"};
        return (f==Family::Nintendo ? nintendo : f==Family::PlayStation ? ps : xbox)[button];
    }
    switch(button) {
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:return f==Family::PlayStation ? "L1" : f==Family::Nintendo ? "L" : "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:return f==Family::PlayStation ? "R1" : f==Family::Nintendo ? "R" : "RB";
    case SDL_GAMEPAD_BUTTON_BACK:
        if(f==Family::PlayStation) {
            auto *pad=ui::Layer::get().renderer().gamepad();const auto type=SDL_GetGamepadType(pad);
            return type==SDL_GAMEPAD_TYPE_PS3 ? "SEL" : type==SDL_GAMEPAD_TYPE_PS4 ? "SHR" : "CRT";
        }
        return f==Family::Nintendo ? "-" : "VW";
    case SDL_GAMEPAD_BUTTON_START:
        if(f==Family::PlayStation && SDL_GetGamepadType(ui::Layer::get().renderer().gamepad())==SDL_GAMEPAD_TYPE_PS3)return "STA";
        return f==Family::PlayStation ? "OPT" : f==Family::Nintendo ? "+" : "MNU";
    case SDL_GAMEPAD_BUTTON_DPAD_UP:return "UP";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:return "DN";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:return "LT";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:return "RT";
    default:
        if(button>=15 && button<=19)return button==15 ? "M1" : "P"+std::to_string(button-15);
        if(button==20)return "TP";
        return "--";
    }
}
std::string mapped_label(input::Action action) {
    if(action==input::Action::StickUp || action==input::Action::Up) {
        const bool analog=action==input::Action::StickUp;
        if(analog && family()!=Family::Keyboard)return "LS";
        const input::Action actions[]{analog ? input::Action::StickUp : input::Action::Up,
            analog ? input::Action::StickLeft : input::Action::Left,
            analog ? input::Action::StickDown : input::Action::Down,
            analog ? input::Action::StickRight : input::Action::Right};
        std::string result="D:";
        for(auto a:actions){if(result.size()>2)result+='|';result+=mapped_single_label(a);}
        return result;
    }
    return mapped_single_label(action);
}
float draw(Action action,float x,float y,float size) {
    auto *d=ImGui::GetForegroundDrawList();const auto text=current_label(action);
    const auto device=family();
    const float font=size*11.f/14.f;
    const auto extent=ImGui::GetFont()->CalcTextSizeA(font,10000,0,text.c_str());
    const bool ps_symbol=device==Family::PlayStation && (text=="Cross" || text=="Circle" || text=="Square" || text=="Triangle");
    const bool face=(device==Family::Nintendo || device==Family::Xbox) && text.size()==1 && std::string_view("ABXY").find(text[0])!=std::string_view::npos;
    if(ps_symbol || face) {
        const ImVec2 center{x+size*.5f,y+size*.5f};const float radius=size*.43f;
        d->AddCircleFilled(center,radius,device==Family::Xbox ? IM_COL32(36,40,44,255) : IM_COL32(27,31,35,255),24);
        d->AddCircle(center,radius,device==Family::Xbox ? IM_COL32(10,12,14,255) : IM_COL32(202,207,211,255),24,std::max(1.f,size*.055f));
        if(ps_symbol) {
            const ImU32 ink=text=="Cross" ? IM_COL32(92,178,238,255) : text=="Circle" ? IM_COL32(238,92,105,255) : text=="Square" ? IM_COL32(221,112,185,255) : IM_COL32(120,208,135,255);
            const float r=radius*.43f,w=std::max(1.f,size*.075f);
            if(text=="Cross") {d->AddLine({center.x-r,center.y-r},{center.x+r,center.y+r},ink,w);d->AddLine({center.x+r,center.y-r},{center.x-r,center.y+r},ink,w);}
            else if(text=="Circle")d->AddCircle(center,r*.78f,ink,20,w);
            else if(text=="Square")d->AddRect({center.x-r*.72f,center.y-r*.72f},{center.x+r*.72f,center.y+r*.72f},ink,0,0,w);
            else {const ImVec2 pts[]{{center.x,center.y-r},{center.x+r,center.y+r*.78f},{center.x-r,center.y+r*.78f}};d->AddPolyline(pts,3,ink,ImDrawFlags_Closed,w);}
        } else {
            ImU32 fill=IM_COL32(229,233,236,255),ink=IM_COL32(36,40,44,255);
            if(device==Family::Xbox) {
                fill=text=="A" ? IM_COL32(70,184,94,255) : text=="B" ? IM_COL32(217,65,67,255) : text=="X" ? IM_COL32(68,139,216,255) : IM_COL32(218,188,61,255);
                ink=IM_COL32(255,255,255,255);
            }
            d->AddCircleFilled(center,radius*.76f,fill,20);
            d->AddText(ImGui::GetFont(),font*.8f,{center.x-extent.x*.5f,center.y-extent.y*.5f},ink,text.c_str());
        }
        return size;
    }
    const float w=std::max(size*22.f/14.f,extent.x+size*10.f/14.f);
    if(device==Family::Keyboard) {
        const float lip=std::max(1.f,size*.1f),round=size*.16f;
        d->AddRectFilled({x+lip,y+lip},{x+w,y+size+lip},IM_COL32(18,23,25,230),round);
        d->AddRectFilled({x,y},{x+w-lip,y+size-lip},IM_COL32(207,214,218,255),round);
        d->AddRectFilled({x+lip,y+lip},{x+w-size*.045f,y+size-size*.14f},IM_COL32(248,249,247,255),round*.72f);
        d->AddRect({x,y},{x+w-lip,y+size-lip},IM_COL32(92,103,109,255),round,0,std::max(1.f,size*.06f));
        d->AddText(ImGui::GetFont(),font,{x+(w-lip-extent.x)*.5f,y+(size-lip-extent.y)*.5f},IM_COL32(35,43,47,255),text.c_str());
    } else {
        const ImU32 face_color=device==Family::PlayStation ? IM_COL32(211,215,220,255) : IM_COL32(57,64,69,255);
        const ImU32 ink=device==Family::PlayStation ? IM_COL32(49,54,60,255) : IM_COL32(245,245,235,255);
        d->AddRectFilled({x+size*.06f,y+size*.08f},{x+w,y+size},IM_COL32(10,13,15,255),size*.28f);
        d->AddRectFilled({x,y},{x+w-size*.05f,y+size*.88f},face_color,size*.27f);
        d->AddRect({x,y},{x+w-size*.05f,y+size*.88f},device==Family::PlayStation ? IM_COL32(242,243,244,255) : IM_COL32(133,145,151,255),size*.27f,0,std::max(1.f,size*.055f));
        d->AddText(ImGui::GetFont(),font,{x+(w-size*.05f-extent.x)*.5f,y+(size*.88f-extent.y)*.5f},ink,text.c_str());
    }
    return w;
}
void register_renderer() {
    static const bool registered=mod_overlay::register_provider({"controller-prompts",{"controller_prompts","native_button_prompts"},enable,disable,visible,render});
    (void)registered;
}
}
