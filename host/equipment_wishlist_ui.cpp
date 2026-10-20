#include "equipment_wishlist.hpp"
#include "wishlist_scene.hpp"
#include "wishlist_feedback.hpp"
#include "gpu/ge_state.hpp"
#include "psprecomp/guest_memory.hpp"
#include "fu_text_table.hpp"
#include "install/user_data.hpp"
#include "mod_overlay.hpp"
#include "ui/layer.hpp"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <chrono>
#include <cstdio>
#include <cstdlib>
namespace portablekit::fu::wishlist {
namespace {
Notifications notifications;
FeedbackTimer feedback;
SceneGate scene;
bool scene_enabled{};
bool ui_quest{};
double scene_clock(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
double crafted_notice_age=5.;
bool presented(const View &state){return (state.quest || state.village) && scene.visible(state.village,scene_clock());}
std::string notification_hunter;
ControlHintProvider hint_provider{};
std::map<unsigned,std::string> item_names;
auto next_names_check=std::chrono::steady_clock::time_point{};
std::filesystem::file_time_type names_stamp{};
void refresh_names() {
    const auto now=std::chrono::steady_clock::now();if(now<next_names_check)return;
    next_names_check=now+std::chrono::seconds(1);
    const auto path=install::user_data_directory()/"save-preview/equipment-names.bin";
    std::error_code ec;const auto stamp=std::filesystem::last_write_time(path,ec);
    if(ec || stamp==names_stamp || std::filesystem::file_size(path,ec)>4*1024*1024 || ec)return;
    std::ifstream in(path,std::ios::binary);const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in),{});
    const auto table=text::read_table(bytes);if(!table || table->sections.size()<2)return;
    for(unsigned id=1;id<std::min<std::size_t>(item_limit,table->sections[1].size());id++)
        if(const auto &value=table->sections[1][id]){item_names[id]=*value;item_name(id,*value);}
    names_stamp=stamp;
}
void enable(const std::filesystem::path &) {
    scene_enabled=true;scene={};feedback.clear();ui_quest=false;
    notifications.clear();notification_hunter.clear();item_names.clear();names_stamp={};next_names_check={};
    set_enabled(true,install::user_data_directory()/"wishlist");refresh_names();
    std::puts("[wishlist] enabled");std::fflush(stdout);
}
void disable() {scene_enabled=false;set_enabled(false);notifications.clear();feedback.clear();scene={};}
bool visible() {
    refresh_names();const auto state=view();
    if(std::getenv("MHFU_WISHLIST_TRACE")) {
        static bool previous=false;const bool shown=presented(state);
        if(previous!=shown){std::printf("[wishlist-visibility] visible=%u village=%u quest=%u notices=%zu time=%.3f\n",shown,state.village,state.quest,state.notices.size(),scene_clock());std::fflush(stdout);previous=shown;}
    }
    if(state.hunter!=notification_hunter){notifications.clear();feedback.clear();notification_hunter=state.hunter;}
    if(ui_quest!=state.quest){notifications.clear();ui_quest=state.quest;}
    if(!state.quest && !state.finish_notices)notifications.clear();
    if(wishlist::has_crafted_notice() || crafted_notice_age<5.)return true;
    if(!presented(state))return false;
    return state.smithy || state.quest || (state.village &&
        (!state.notices.empty() || !notifications.cards().empty() || notifications.queued() || feedback.visible() || has_pin_feedback()));
}
void pin_icon(ImDrawList *d,ImVec2 c,float size,ImU32 color) {
    const auto p=[&](float x,float y){return ImVec2{c.x+x*size,c.y+y*size};};
    d->AddLine(p(-.5f,.58f),p(.18f,-.1f),IM_COL32(8,12,12,255),size*.3f);
    d->AddLine(p(-.5f,.58f),p(.18f,-.1f),color,std::max(1.f,size*.14f));
    const ImVec2 head[]{p(-.28f,-.45f),p(.12f,-.8f),p(.65f,-.27f),p(.3f,.1f),p(.08f,-.13f),p(-.25f,.18f),p(-.48f,-.05f),p(-.05f,-.35f)};
    d->AddConcavePolyFilled(head,8,color);d->AddPolyline(head,8,IM_COL32(8,12,12,255),ImDrawFlags_Closed,std::max(.6f,size*.1f));
}
void draw_hint(ImDrawList *d,const ControlHint &hint,const char *action,ImVec2 at,float scale) {
    const float font_size=12*scale;
    float width=(hint.width>0 ? hint.width : 14.f)*scale;
    if(hint.draw)hint.draw(at.x,at.y,14*scale);
    else {
        const auto *label=hint.label.c_str();width=ImGui::GetFont()->CalcTextSizeA(font_size,10000,0,label).x;
        d->AddText(ImGui::GetFont(),font_size,at,IM_COL32(255,215,125,255),label);
    }
    d->AddText(ImGui::GetFont(),font_size,{at.x+width+8*scale,at.y+scale},IM_COL32(0,0,0,230),action);
    d->AddText(ImGui::GetFont(),font_size,{at.x+width+7*scale,at.y},IM_COL32(238,238,228,255),action);
}
float hint_width(const ControlHint &hint,const char *action,float scale) {
    const auto text_width=[&](const char *text){return ImGui::GetFont()->CalcTextSizeA(12*scale,10000,0,text).x;};
    const float button=hint.draw ? (hint.width>0 ? hint.width : 14.f)*scale : text_width(hint.label.c_str());
    return button+7*scale+text_width(action);
}
void draw() {
    refresh_names();auto state=view();const bool scene_visible=presented(state);
    if(!scene_visible && !has_crafted_notice() && crafted_notice_age>=5.)return;
    state=view(true);
    auto *d=ImGui::GetForegroundDrawList();const auto *vp=ImGui::GetMainViewport();
    const float sx=vp->Size.x/480.f,sy=vp->Size.y/272.f,scale=std::min(sx,sy);
    if(auto notice=take_pin_feedback())feedback.show(std::move(*notice));
    else feedback.advance(ImGui::GetIO().DeltaTime);
    if(feedback.visible()) {
        const auto &notice=feedback.value();const std::string label=(notice.pinned ? "Pinned: " : "Unpinned: ")+notice.equipment;
        const float max_width=194*scale;float font=10*scale;
        auto extent=ImGui::GetFont()->CalcTextSizeA(font,10000,0,label.c_str());
        if(extent.x>max_width-22*scale)font=std::max(8*scale,font*(max_width-22*scale)/extent.x);
        extent=ImGui::GetFont()->CalcTextSizeA(font,10000,0,label.c_str());
        const ImVec2 at{vp->Pos.x+6*scale,vp->Pos.y+scale};
        const float width=std::min(max_width,extent.x+22*scale),height=14*scale;
        const auto color=notice.pinned ? IM_COL32(170,235,140,255) : IM_COL32(245,211,144,255);
        d->AddRectFilled(at,{at.x+width,at.y+height},IM_COL32(12,21,19,215),3*scale);
        pin_icon(d,{at.x+8*scale,at.y+7*scale},4*scale,color);
        d->PushClipRect(at,{at.x+width-3*scale,at.y+height},true);
        d->AddText(ImGui::GetFont(),font,{at.x+17*scale,at.y+(height-font)*.5f},color,label.c_str());
        d->PopClipRect();
    }
    if(take_crafted_notice())crafted_notice_age=0.;
    else crafted_notice_age+=std::max(0.f,ImGui::GetIO().DeltaTime);
    if(crafted_notice_age<5.) {
        const auto animation=motion(crafted_notice_age);
        const float width=std::min(vp->Size.x*.48f,205*scale),height=26*scale;
        const float font_size=11*scale;
        const char *message="Wishlisted equipment crafted";
        const auto extent=ImGui::GetFont()->CalcTextSizeA(font_size,10000,0,message);
        const float text_size=extent.x>width-18*scale ? font_size*(width-18*scale)/extent.x : font_size;
        const float alpha=animation.alpha;
        const ImVec2 at{vp->Pos.x+14*scale,
            vp->Pos.y+vp->Size.y*.5f-height*.5f+animation.offset*scale};
        d->AddRectFilled(at,{at.x+width,at.y+height},IM_COL32(12,21,19,int(225*alpha)),4*scale);
        d->AddText(ImGui::GetFont(),text_size,{at.x+9*scale,at.y+(height-text_size)*.5f},
            IM_COL32(245,244,231,int(255*alpha)),message);
    }
    if(!scene_visible)return;
    if(state.smithy) {
        for(auto marker:state.markers)pin_icon(d,{vp->Pos.x+marker.x*sx,vp->Pos.y+marker.y*sy},5.5f*scale,IM_COL32(255,213,93,255));
        const auto hints=hint_provider ? hint_provider() : ControlHints{};
        const float pin_width=hint_width(hints.pin,"Pin materials",scale);
        const float total=pin_width+18*scale+hint_width(hints.unpin,"Remove pin",scale);
        const ImVec2 at{vp->Pos.x+vp->Size.x-6*scale-total,vp->Pos.y+vp->Size.y-15*scale};
        draw_hint(d,hints.pin,"Pin materials",at,scale);
        draw_hint(d,hints.unpin,"Remove pin",{at.x+pin_width+18*scale,at.y},scale);
    }
    if(!state.quest && !state.finish_notices){notifications.clear();return;}
    for(auto &notice:state.notices)notifications.push(std::move(notice));
    notifications.advance(ImGui::GetIO().DeltaTime);
    const float width=std::min(190*scale,vp->Size.x*.4f),height=38*scale,gap=5*scale;
    const float x=vp->Pos.x+14*scale,anchor=vp->Pos.y+vp->Size.y*.5f;
    for(const auto &card:notifications.cards()) {
        const auto animation=motion(card.age);if(animation.alpha<=0)continue;
        const auto alpha=static_cast<int>(255*animation.alpha);
        const float y=anchor+card.y*(height+gap)+animation.offset*scale;
        const auto accent=card.notice.complete ? IM_COL32(158,226,119,alpha) : IM_COL32(248,207,111,alpha);
        d->AddRectFilled({x,y},{x+width,y+height},IM_COL32(12,21,19,int(225*animation.alpha)),5*scale);
        d->AddRectFilled({x,y+5*scale},{x+3*scale,y+height-5*scale},accent,scale);
        std::string title,detail;
        if(card.notice.complete){title="Materials for";detail=card.notice.equipment+" acquired";}
        else {
            title="Wishlist item acquired";
            detail=item_names.contains(card.notice.item) ? item_names[card.notice.item] : "Wishlist material";
            detail+="  +"+std::to_string(card.notice.quantity);
        }
        auto *font=ImGui::GetFont();const float title_size=10*scale;
        d->AddText(font,title_size,{x+11*scale,y+5*scale},accent,title.c_str());
        // Shrink long equipment names within the single readable detail line.
        float detail_size=11*scale;
        const auto extent=font->CalcTextSizeA(detail_size,10000,0,detail.c_str());
        if(extent.x>width-22*scale)detail_size=std::max(10*scale,detail_size*(width-22*scale)/extent.x);
        d->PushClipRect({x+8*scale,y},{x+width-8*scale,y+height},true);
        d->AddText(font,detail_size,{x+11*scale,y+21*scale},IM_COL32(245,244,231,alpha),detail.c_str());
        d->PopClipRect();
    }
}
}
bool panel(bool first_frame) {
    static std::optional<Key> expanded;
    if(first_frame)expanded.reset();
    if(!mod_overlay::is_active("equipment-wishlist"))return false;
    refresh_names();const auto state=view();
    auto &layer=ui::Layer::get();const float scale=layer.scale();
    const auto *vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowSize({std::min(vp->Size.x-24*scale,650*scale),std::min(vp->Size.y-24*scale,520*scale)},ImGuiCond_Always);
    ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x*.5f,vp->Pos.y+vp->Size.y*.5f},ImGuiCond_Always,{.5f,.5f});
    if(first_frame)ImGui::SetNextWindowFocus();
    bool open=true;
    if(ImGui::Begin("Equipment Wishlist##window",&open,ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextUnformatted("Select or confirm an item to view materials you have and still need.");
        ImGui::Separator();
        ImGui::BeginChild("wishlist-items",{0,-ImGui::GetFrameHeightWithSpacing()*1.8f},ImGuiChildFlags_Borders);
        if(state.wishlist.empty())ImGui::TextWrapped("No equipment is wishlisted yet. Pin a recipe at the smithy to add it here.");
        for(std::size_t i=0;i<state.wishlist.size();i++) {
            const auto &item=state.wishlist[i];
            const bool ready=std::all_of(item.materials.begin(),item.materials.end(),[](const Requirement &material){return material.missing==0;});
            const bool selected=expanded && *expanded==item.key;
            const std::string label=item.equipment+(ready?"  [Ready to craft]":"");
            ImGui::PushID(static_cast<int>(i));
            ImGui::Selectable(label.c_str(),selected,ImGuiSelectableFlags_None,{0,ImGui::GetTextLineHeightWithSpacing()*1.6f});
            if(first_frame && i==0)ImGui::SetItemDefaultFocus();
            if(ImGui::IsItemActivated())expanded=selected?std::optional<Key>{}:std::optional<Key>{item.key};
            ImGui::PopID();
            if(!expanded || *expanded!=item.key)continue;
            ImGui::Indent(12*scale);
            if(ImGui::BeginTable("wishlist-materials",4,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Material",ImGuiTableColumnFlags_WidthStretch,2.4f);
                ImGui::TableSetupColumn("Have",ImGuiTableColumnFlags_WidthStretch,.8f);
                ImGui::TableSetupColumn("Needed",ImGuiTableColumnFlags_WidthStretch,.8f);
                ImGui::TableSetupColumn("Missing",ImGuiTableColumnFlags_WidthStretch,.9f);
                ImGui::TableHeadersRow();
                for(const auto &material:item.materials) {
                    const auto name=item_names.contains(material.item)?item_names.at(material.item):"#"+std::to_string(material.item);
                    ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(name.c_str());
                    ImGui::TableNextColumn();ImGui::Text("%u",material.owned);
                    ImGui::TableNextColumn();ImGui::Text("%u",material.required);
                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text,material.missing?IM_COL32(245,133,117,255):IM_COL32(151,223,137,255));
                    ImGui::Text("%u",material.missing);ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }
            ImGui::Unindent(12*scale);ImGui::Spacing();
        }
        ImGui::EndChild();
        if(ImGui::Button("Close"))open=false;
        ImGui::SameLine();ImGui::TextDisabled("F7 / Esc / controller cancel to close");
        const auto cancel=ui::Layer::get().confirm_south()?ImGuiKey_GamepadFaceRight:ImGuiKey_GamepadFaceDown;
        if(ui::Layer::get().gamepad_armed() && ImGui::IsKeyPressed(cancel,false))open=false;
    }
    ImGui::End();
    return open;
}
void set_control_hint_provider(ControlHintProvider provider) {hint_provider=provider;}
void observe_scene(const gpu::DrawCall &call,const psprecomp::GuestMemory &memory) {
    if(!scene_enabled || call.clear_mode)return;
    if(!call.through && (!call.vertices.empty() || call.raw_count)) {
        const bool town=memory.load32(0x09AA5770)==0x27BDFDC0 && memory.load32(0x09AA49F8)==0x27BDFFF0;
        if(std::getenv("MHFU_WISHLIST_TRACE")){static int previous=-1;if(previous!=int(town)){previous=int(town);std::printf("[wishlist-scene] world village=%u vertices=%zu raw=%u\n",town,call.vertices.size(),call.raw_count);std::fflush(stdout);}}
        scene.world(town,scene_clock());return;
    }
    if(!call.through || call.vertices.size()<2)return;
    // A framebuffer copy presents the world itself, rather than covering it.
    if(call.texture.enabled && (call.texture.address&0x0fffffffu)<0x08000000u)return;
    const auto cover=[&](std::size_t start,std::size_t count) {
        float left=10000,top=10000,right=-10000,bottom=-10000;
        unsigned alpha=call.has_vertex_color ? 0 : call.material_color>>24;
        unsigned low_alpha=call.has_vertex_color ? 255 : alpha;
        bool black=call.has_vertex_color || !(call.material_color&0x00ffffffu);
        for(std::size_t i=start;i<start+count;++i) {const auto &v=call.vertices[i];
            left=std::min(left,v.position[0]);right=std::max(right,v.position[0]);
            top=std::min(top,v.position[1]);bottom=std::max(bottom,v.position[1]);
            if(call.has_vertex_color){alpha=std::max(alpha,v.color>>24);low_alpha=std::min(low_alpha,v.color>>24);black&=!(v.color&0x00ffffffu);}
        }
        if(left<=1 && top<=1 && right>=479 && bottom>=271 && blocks_scene(call.texture.enabled,low_alpha,alpha,black)) {
            scene.cover();
            if(std::getenv("MHFU_WISHLIST_TRACE")) {
                static std::string previous;const auto key=std::to_string(call.texture.address)+":"+std::to_string(alpha)+":"+std::to_string(call.vertices.size());
                if(previous!=key){previous=key;std::printf("[wishlist-scene] cover texture=%08X textured=%u alpha=%u material=%08X color=%08X blend=%u,%u vertices=%zu\n",call.texture.address,call.texture.enabled,alpha,call.material_color,call.vertices[0].color,call.blend.source_factor,call.blend.destination_factor,call.vertices.size());std::fflush(stdout);}
            }
        }
    };
    // A batch of scattered HUD sprites is not a full-screen cover.
    if(call.primitive==gpu::PrimitiveType::Sprites) {
        for(std::size_t i=0;i+1<call.vertices.size();i+=2)cover(i,2);
    } else if(call.vertices.size()<=6)cover(0,call.vertices.size());
}
void register_renderer() {
    static const bool registered=mod_overlay::register_provider({"equipment-wishlist",{"equipment_wishlist","controller_hint_provider"},enable,disable,visible,draw});
    (void)registered;
}
}
