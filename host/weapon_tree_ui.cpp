#include "weapon_tree.hpp"
#include "equipment_preview.hpp"
#include "equipment_wishlist.hpp"
#include "mod_overlay.hpp"
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "imgui.h"
#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
namespace portablekit::fu::weapon_tree {
namespace {
wishlist::Key wishlist_key(const Weapon &w) {
    return {w.operation,1,w.recipe,w.type,w.id};
}
wishlist::Recipe wishlist_recipe(const Weapon &w,const std::string &name) {
    wishlist::Recipe recipe{wishlist_key(w),name,{}};
    for(unsigned i=0;i<w.material_count;i++)recipe.materials.push_back({w.materials[i].item,w.materials[i].quantity});
    return recipe;
}
void pin_mark(ImDrawList *draw,ImVec2 at,float scale) {
    const auto color=IM_COL32(97,206,121,255);
    draw->AddCircleFilled({at.x+7*scale,at.y+6*scale},4*scale,color,12);
    draw->AddLine({at.x+9*scale,at.y+9*scale},{at.x+4*scale,at.y+15*scale},color,2*scale);
    draw->AddLine({at.x+2*scale,at.y+16*scale},{at.x+7*scale,at.y+16*scale},color,2*scale);
}
void check_mark(ImDrawList *draw,ImVec2 at,float scale) {
    const auto green=IM_COL32(77,190,87,255);
    draw->AddLine({at.x,at.y+5*scale},{at.x+4*scale,at.y+9*scale},green,2.3f*scale);
    draw->AddLine({at.x+4*scale,at.y+9*scale},{at.x+12*scale,at.y},green,2.3f*scale);
}
const char *rapid_fire_ammo(const Weapon &w) {
    // Rapid fire is not stored in the bowgun records. These entries are the
    // game's Light Bowgun roster for ULUS10391, keyed by gunner weapon ID
    // (type 6), so they hold in any language.
    if(w.type!=6)return "None";
    static const std::unordered_map<unsigned,const char *> entries{
        {2,"Normal S Lv1"},{3,"Pellet S Lv1"},{10,"Normal S Lv2"},          // Chain Blitz, Shotgun (White), Jade Storm
        {168,"Pierce S Lv1"},{21,"Crag S Lv1"},{8,"Normal S Lv2"},          // Assault Conga, Grenade Launcher, Sandfall
        {9,"Water S"},{13,"Freeze S"},{11,"Water S"},                        // Maelstrom, Tail String, Lobster Gun
        {19,"Thunder S"},{14,"Freeze S"},                                    // Sakura Parasol, Tail String+
        {23,"Flaming S"},{29,"Normal S Lv1, Flaming S"},                     // Uranos Grenade, Felyne Ragdoll
        {30,"Normal S Lv1, Flaming S"},{31,"Thunder S"},                     // Melynx Ragdoll, Black Belt Bowgun
        {25,"Normal S Lv2"},{28,"Normal S Lv2, Pierce S Lv1"},               // Valkyrie Heart, Azure Sakura
        {27,"Pierce S Lv1"},{50,"Crag S Lv1"},                               // Spartacus Soul, Grenade Launcher+
        {170,"Pierce S Lv1"},{43,"Water S"},                                 // Assault Conga+, Maelstrom+
        {64,"Normal S Lv1, Flaming S"},                                      // Felyne Helldoll
        {65,"Normal S Lv1, Flaming S"},{169,"Dragon S"},                     // Melynx Helldoll, Shakalaka Prized Bow
        {66,"Thunder S"},{55,"Normal S Lv2"},                                // Expert Bowgun, Valkyrie Heart+
        {58,"Pierce S Lv1"},{61,"Normal S Lv2, Pierce S Lv1"},               // Spartacus Soul+, Supreme Azure Sakura
        {56,"Pellet S Lv1"},{62,"Pellet S Lv1, Pierce S Lv2"},               // Gold Valkyrie, Profusion
        {63,"Flaming S, Pierce S Lv1"},{53,"Crag S Lv1"},                    // Fire Wrath, Rajang Barrage
        {172,"Normal S Lv1"},{173,"Water S"},{174,"Normal S Lv2"},           // Hi Chain Blitz, Nebula Storm, Jade Typhoon
        {175,"Water S"},{178,"Pierce S Lv1"},                                // Royal Lobster Gun, Combat Conga
        {179,"Pellet S Lv2"},{177,"Recovery S Lv1"},                         // Jungle Assault, Heartfelt Cast
        {188,"Pellet S Lv3"},{45,"Water S"},                                 // Shotgun (Snake), King Lobster Gun
        {42,"Normal S Lv2"},{194,"Pierce S Lv3"},                            // Sandfall+, Lava Storm
        {195,"Normal S Lv3"},{182,"Crag S Lv1"},                             // Desert Tail, Owl's Eye Darkbow
        {184,"Thunder S"},{191,"Normal S Lv2, Pierce S Lv1"},                // Warrior's Bowgun, Pollination
        {196,"Clust S Lv1"},{197,"Pellet S Lv1"},                            // Mythical Three-Horn, Gold Semi-Auto
        {198,"Pierce S Lv2"},{199,"Pellet S Lv1, Pierce S Lv2"},             // Silver Semi-Auto, Prosperity
        {202,"Flaming S"},{203,"Pierce S Lv1, Flaming S"},                   // Chronos Panzer, Engraved Fire Wyvern Bow
        {204,"Crag S Lv1"},                                                  // Ten-Thousand Volts
        {206,"Pierce S Lv1, Flaming S, Water S, Thunder S, Freeze S"},       // Absolute Bow
        {210,"Pellet S Lv1"},{216,"Normal S Lv2"},                           // Shotgun (White) G, Sandfall G
        {222,"Normal S Lv2"}                                                 // Jade Typhoon G
    };
    const auto found=entries.find(w.id);
    return found==entries.end()?"None":found->second;
}
void enable(const std::filesystem::path &) {set_enabled(true);}
void disable() {set_enabled(false);}
bool visible() {return available();}
void hint() {
    const auto *vp=ImGui::GetMainViewport();
    const auto text="F8  Weapon upgrade tree";
    const auto size=ImGui::CalcTextSize(text);
    const ImVec2 at{vp->Pos.x+18,vp->Pos.y+vp->Size.y-size.y-12};
    auto *d=ImGui::GetForegroundDrawList();
    d->AddText({at.x+1,at.y+1},IM_COL32(0,0,0,255),text);
    d->AddText(at,IM_COL32(235,242,224,255),text);
}
void stats(const Weapon &w,const std::string &name) {
    ImGui::TextUnformatted(name.c_str());ImGui::Separator();
    ImGui::Text("Rarity %u",w.rarity+1);ImGui::Text("Power: %u",w.power);
    constexpr const char *effects[]={"None","Fire","Water","Thunder","Dragon","Ice","Poison","Paralysis","Sleep"};
    bool effect=false;
    for(unsigned i=0;i<2;i++)if(w.elements[i] && w.values[i]) {ImGui::Text("%s: %u",effects[w.elements[i]],w.values[i]);effect=true;}
    if(!effect)ImGui::TextUnformatted("Element / status: None");
    if(w.type==6 && w.category!=10) {
        constexpr const char *recoils[]={"Strongest","Strong","Moderate","Light","Weak","Very weak","Weakest"};
        constexpr const char *reloads[]={"Very fast","Fast","Normal","Slow","Very slow"};
        ImGui::Text("Recoil: %s",recoils[w.recoil]);ImGui::Text("Reload: %s",reloads[w.reload]);
        ImGui::Text("Deviation: %.2f",w.deviation);
        ImGui::Separator();ImGui::TextUnformatted("Supported ammo (magazine size)");
        // Each bit follows the game's ammo ordering, including Tranq before Paint.
        constexpr const char *ammo[]={"Normal 1","Normal 2","Normal 3","Pierce 1","Pierce 2","Pierce 3","Pellet 1","Pellet 2","Pellet 3","Crag 1","Crag 2","Crag 3","Clust 1","Clust 2","Clust 3","Flaming","Water","Thunder","Freeze","Dragon","Recovery 1","Recovery 2","Poison 1","Poison 2","Paralysis 1","Paralysis 2","Sleep 1","Sleep 2","Tranq","Paint","Demon","Armor"};
        unsigned row=0;
        if(ImGui::BeginTable("ammo",3,ImGuiTableFlags_SizingFixedFit)) {
            for(unsigned i=0;i<32;i++)if((w.ammo_mask&(1u<<i)) && w.ammo[i]) {
                ImGui::TableNextColumn();ImGui::Text("%s (%u)",ammo[i],w.ammo[i]);row++;
            }
            ImGui::EndTable();
        }
        if(!row)ImGui::TextUnformatted("None");
        if(w.category==1)ImGui::Text("Rapid fire: %s",rapid_fire_ammo(w));
        ImGui::TextDisabled("Base weapon; no skills, attachments or power upgrades.");
    } else if(w.type==6) {
        ImGui::Separator();ImGui::TextUnformatted("Charge shots");
        constexpr const char *shot[]={"Rapid","Spread","Pierce"};
        for(unsigned i=0;i<4;i++)ImGui::Text("Charge %u: %s Lv%u%s",i+1,shot[w.shots[i]/5],w.shots[i]%5+1,i==w.charges?" (Load Up)":i>w.charges?" (Unavailable)":"");
        constexpr const char *coatings[]={"Power","Poison","Paralysis","Sleep","Close-range","Paint"};
        std::string supported;
        for(unsigned i=0;i<6;i++)if(w.ammo_mask&(1u<<(i+1))) {if(!supported.empty())supported+=", ";supported+=coatings[i];}
        ImGui::TextWrapped("Coatings: %s",supported.empty()?"None":supported.c_str());
    }
    if(w.type==5 && w.category==8) {
        constexpr const char *shelling[]={"Normal","Long","Wide"};
        if(w.weapon_variable<=14)ImGui::Text("Shelling: %s Lv%u",shelling[w.weapon_variable%3],w.weapon_variable/3+1);
    }
    if(w.type==5 && w.category==9) {
        static constexpr std::array<std::array<const char *,3>,20> notes{{
            {{"White","Blue","Red"}},{{"White","Yellow","Red"}},{{"White","Green","Red"}},{{"White","Blue","Yellow"}},
            {{"White","Green","Yellow"}},{{"White","Green","Blue"}},{{"Purple","Blue","Red"}},{{"Purple","Yellow","Red"}},
            {{"Purple","Blue","Yellow"}},{{"Purple","Green","Yellow"}},{{"Purple","Green","Blue"}},{{"Purple","Green","Red"}},
            {{"White","Green","Water"}},{{"White","Yellow","Water"}},{{"White","Water","Blue"}},{{"White","Water","Red"}},
            {{"Purple","Green","Water"}},{{"Purple","Yellow","Water"}},{{"Purple","Water","Blue"}},{{"Purple","Water","Red"}}
        }};
        static constexpr const char *songs[]={
            "Attack Up Small, Health Max +20, Wind Pressure Small, Infinite Stamina Small, Self-Movement Up, Self-Improvement",
            "Attack Up Small, Defense Up Small, Fire Resistance Small, Dragon Resistance Small, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Attack Up Small, Defense Up Large, Antidote and Heal +20, Self-Movement Up, Self-Improvement",
            "Infinite Stamina Small, Elemental Attack Up, Fire Resistance Large, Water Resistance Large, Thunder Resistance Large, Dragon Resistance Small, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Antidote and Heal +20, Dragon Resistance Small, Recovery Speed Up Small, Ice Resistance Small, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Wind Pressure Large, Infinite Stamina Small, Antidote and Heal +20, Self-Movement Up, Self-Improvement",
            "Attack Up Large, Infinite Stamina Large, Health Max +50, Wind Pressure Small, Self-Movement Up, Self-Improvement",
            "Attack Up Large, Defense Up Small, Fire Resistance Small, Dragon Resistance Large, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Wind Pressure Negation, Infinite Stamina Large, Water Resistance Small, Thunder Resistance Large, Dragon Resistance Large, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Heal +20, Heal +30, Recovery Speed Up Small, Divine Protection, Dragon Resistance Large, Ice Resistance Large, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Wind Pressure Large, Infinite Stamina Large, Heal +20, Antidote and Heal +30, Self-Movement Up, Self-Improvement",
            "Attack Up Large, Defense Up Large, Heal +20, Recovery Speed Up Large, Self-Movement Up, Self-Improvement",
            "Antidote and Heal +20, Anti-Heat, Freshness and Heal +30, Self-Movement Up, Self-Improvement",
            "Anti-Cold and Freeze, Negate Paralysis, Negate Quake, Dragon Resistance Small, Ice Resistance Small, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Auto-Detect, Wind Pressure Large, Infinite Stamina Small, Self-Movement Up, Self-Improvement",
            "Attack Up Small, Health Max +30, Earplug, Self-Movement Up, Self-Improvement",
            "Heal +20, Heal +60, High-Grade Earplug, Anti-Heat, Self-Movement Up, Self-Improvement",
            "Anti-Cold and Freeze, Negate Paralysis, Negate Quake, Thunder Resistance Small, Dragon Resistance Large, Ice Resistance Large, Supersonic Waves, Self-Movement Up, Self-Improvement",
            "Auto-Detect, Wind Pressure Large, Infinite Stamina Large, Negate Faint, Self-Movement Up, Self-Improvement",
            "Attack Up Large, Defense Up Large, Health Max +30, Earplug, Self-Movement Up, Self-Improvement"
        };
        if(w.weapon_variable<notes.size()) {
            ImGui::Text("Hunting Horn notes: %s, %s, %s",notes[w.weapon_variable][0],notes[w.weapon_variable][1],notes[w.weapon_variable][2]);
            ImGui::TextUnformatted("Available songs");ImGui::TextWrapped("%s",songs[w.weapon_variable]);
        }
    }
    if(w.type==5) {
        ImGui::TextUnformatted("Sharpness");
        const auto at=ImGui::GetCursorScreenPos();const float width=ImGui::GetFontSize()*12,height=ImGui::GetFontSize()*.55f;
        constexpr ImU32 colors[]={IM_COL32(220,50,49,255),IM_COL32(238,132,43,255),IM_COL32(240,213,73,255),IM_COL32(80,184,86,255),IM_COL32(68,132,226,255),IM_COL32(240,240,235,255),IM_COL32(164,89,210,255)};
        auto *d=ImGui::GetWindowDrawList();d->AddRectFilled(at,{at.x+width,at.y+height},IM_COL32(20,24,25,255));
        float x=at.x;
        for(unsigned i=0;i<7;i++){const float next=x+width*w.sharpness[i]/450.f;if(next>x)d->AddRectFilled({x,at.y},{next,at.y+height},colors[i]);x=next;}
        ImGui::Dummy({width,height+2});
    }
    if(!w.owned && w.material_count) {
        ImGui::Separator();ImGui::TextUnformatted(w.operation?"Materials to upgrade":"Materials to craft");
        for(unsigned i=0;i<w.material_count;i++) {
            const auto material=w.materials[i];
            ImGui::BulletText("%s x%u",wishlist::item_name(material.item).c_str(),material.quantity);
        }
    }
}
}
bool panel(bool first) {
    static std::optional<Chart> chart;
    static unsigned selected{};
    static bool focus{};
    static bool controller_tooltip{};
    static unsigned tooltip_weapon{};
    if(first) {chart=snapshot();if(!chart || chart->nodes.empty())return false;selected=chart->selected;focus=true;controller_tooltip=false;tooltip_weapon=0;}
    if(!chart || !available())return false;
    const auto *vp=ImGui::GetMainViewport();const float scale=ImGui::GetFontSize()/20.f;
    ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x*.035f,vp->Pos.y+vp->Size.y*.055f},ImGuiCond_Always);
    ImGui::SetNextWindowSize({vp->Size.x*.93f,vp->Size.y*.89f},ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.98f);
    bool open=true;
    ImGui::Begin("Weapon Upgrade Tree",&open,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted("Follow the branches. Hover a weapon or confirm it with the controller to view its stats.");
    ImGui::TextDisabled("All upgrade paths shown, including those not yet unlocked. Highlighted weapon: smithy selection.");
    const float footer=ImGui::GetFrameHeightWithSpacing()*1.3f;
    ImGui::BeginChild("tree-canvas",{0,-footer},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
    const float bw=260*scale,bh=62*scale,gapx=54*scale,gapy=24*scale;
    std::map<unsigned,ImVec2> positions;
    unsigned max_column=0,max_rows=0;
    for(const auto &n:chart->nodes){max_column=std::max(max_column,n.column);max_rows=std::max(max_rows,n.row+1);}
    const auto origin=ImGui::GetCursorScreenPos();
    for(const auto &n:chart->nodes)positions[n.weapon.id]={origin.x+n.column*(bw+gapx)+12*scale,origin.y+n.row*(bh+gapy)+12*scale};
    auto *draw=ImGui::GetWindowDrawList();
    for(auto edge:chart->edges) {
        auto a=positions.at(edge[0]),b=positions.at(edge[1]);a.x+=bw;a.y+=bh*.5f;b.y+=bh*.5f;
        const float mid=(a.x+b.x)*.5f;
        draw->AddLine(a,{mid,a.y},IM_COL32(115,152,126,255),2*scale);
        draw->AddLine({mid,a.y},{mid,b.y},IM_COL32(115,152,126,255),2*scale);
        draw->AddLine({mid,b.y},b,IM_COL32(115,152,126,255),2*scale);
        draw->AddTriangleFilled(b,{b.x-7*scale,b.y-4*scale},{b.x-7*scale,b.y+4*scale},IM_COL32(167,202,173,255));
    }
    for(const auto &n:chart->nodes) {
        const EquippedItem item{true,n.weapon.type,n.weapon.id};const auto name=equipment::name(item);
        const auto at=positions.at(n.weapon.id);ImGui::SetCursorScreenPos(at);ImGui::PushID(int(n.weapon.id));
        const bool original=n.weapon.id==chart->selected;
        ImGui::PushStyleColor(ImGuiCol_Button,original?IM_COL32(29,100,60,255):IM_COL32(30,40,43,255));
        ImGui::Button("##weapon",{bw,bh});ImGui::PopStyleColor();
        if(focus && n.weapon.id==selected) {ImGui::SetKeyboardFocusHere(-1);ImGui::SetScrollHereX(.5f);ImGui::SetScrollHereY(.5f);focus=false;}
        const bool hovered=ImGui::IsItemHovered(),focused=ImGui::IsItemFocused();
        if(hovered)selected=n.weapon.id;
        if(focused) {selected=n.weapon.id;ImGui::SetScrollHereX(.5f);ImGui::SetScrollHereY(.5f);}
        equipment::draw_icon(item,at.x+10*scale,at.y+(bh-30*scale)*.5f,30*scale,draw);
        if(n.weapon.owned)check_mark(draw,{at.x+bw-25*scale,at.y+bh-18*scale},scale);
        const bool wishlist_active=mod_overlay::is_active("equipment-wishlist");
        const auto recipe=wishlist_recipe(n.weapon,name);
        const bool pinned=wishlist_active && n.weapon.material_count && wishlist::pinned(recipe.key);
        if(pinned)pin_mark(draw,{at.x+bw-25*scale,at.y+5*scale},scale);
        draw->PushClipRect({at.x+48*scale,at.y},{at.x+bw-8*scale,at.y+bh},true);
        draw->AddText({at.x+50*scale,at.y+10*scale},IM_COL32(240,241,230,255),name.c_str());
        const auto rarity="Rarity "+std::to_string(n.weapon.rarity+1);
        draw->AddText({at.x+50*scale,at.y+bh-24*scale},IM_COL32(160,185,170,255),rarity.c_str());draw->PopClipRect();
        if(hovered)controller_tooltip=false;
        const auto mouse_delta=ImGui::GetIO().MouseDelta;
        if(controller_tooltip && !hovered && (mouse_delta.x!=0 || mouse_delta.y!=0))controller_tooltip=false;
        if(focused && !hovered && ImGui::IsItemActivated()) {
            if(controller_tooltip && tooltip_weapon==n.weapon.id)controller_tooltip=false;
            else {controller_tooltip=true;tooltip_weapon=n.weapon.id;}
        }
        if(controller_tooltip && (!focused || tooltip_weapon!=n.weapon.id))controller_tooltip=false;
        if(hovered || (focused && controller_tooltip)) {
            if(focused && !hovered) {
                const bool gun=n.weapon.type==6 && n.weapon.category!=10;
                unsigned ammo_count=0;
                if(gun)for(unsigned i=0;i<32;i++)if((n.weapon.ammo_mask&(1u<<i)) && n.weapon.ammo[i])ammo_count++;
                const float width=(gun?520.f:300.f)*scale;
        const float height=gun?(13+(ammo_count+2)/3)*ImGui::GetTextLineHeightWithSpacing()+2*ImGui::GetStyle().WindowPadding.y:300*scale;
                const float left=vp->Pos.x+8,top=vp->Pos.y+8;
                ImGui::SetNextWindowPos({std::clamp(at.x,left,std::max(left,vp->Pos.x+vp->Size.x-width-8)),
                    std::clamp(at.y+bh+10*scale,top,std::max(top,vp->Pos.y+vp->Size.y-height-8))},ImGuiCond_Always);
            }
            ImGui::BeginTooltip();stats(n.weapon,name);ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos({origin.x+(max_column+1)*(bw+gapx),origin.y+max_rows*(bh+gapy)});ImGui::Dummy({1,1});
    ImGui::EndChild();
    if(mod_overlay::is_active("equipment-wishlist")) {
        const auto found=std::find_if(chart->nodes.begin(),chart->nodes.end(),[&](const Node &node){return node.weapon.id==selected;});
        if(found!=chart->nodes.end()) {
            const auto item=EquippedItem{true,found->weapon.type,found->weapon.id};
            const auto recipe=wishlist_recipe(found->weapon,equipment::name(item));
            const bool pinned=found->weapon.material_count && wishlist::pinned(recipe.key);
            ImGui::BeginDisabled(!found->weapon.material_count);
            if(ImGui::Button(pinned?"Remove selected from Wishlist":"Add selected to Wishlist"))wishlist::toggle_pin(recipe);
            ImGui::EndDisabled();ImGui::SameLine();
        }
    }
    if(ImGui::Button("Close"))open=false;ImGui::SameLine();ImGui::TextDisabled("F8 / Esc / controller Back to close");
    ImGui::End();return open;
}
void register_renderer() {static const bool registered=mod_overlay::register_provider({"weapon-upgrade-tree",{"weapon_upgrade_tree"},enable,disable,visible,hint});(void)registered;}
}
#else
namespace portablekit::fu::weapon_tree {bool panel(bool){return false;}void register_renderer(){}}
#endif
