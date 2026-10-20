#include "save_preview.hpp"
#include "save_portraits.hpp"
#include "direct_load.hpp"
#include "equipment_preview.hpp"
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "save_data/save_transfer.hpp"
#include "ui/host_text.hpp"
#include "ui/layer.hpp"
#include "ui/widgets.hpp"
#include "imgui.h"

#include <chrono>
#include <algorithm>

namespace portablekit::fu {
void draw_save_preview(float x, float y, float width, float height, bool refresh) {
    if(refresh) direct_load::reset_request();
    static SavePreview summary;
    static std::filesystem::path root;
    static std::filesystem::file_time_type modified{};
    static std::filesystem::file_time_type summarised{};
    static auto next_check = std::chrono::steady_clock::time_point{};
    const auto now=std::chrono::steady_clock::now();
    if(refresh || now>=next_check) {
        const auto current=savedata::memory_stick();
        std::error_code ec;
        const auto stamp=std::filesystem::last_write_time(current/"PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN",ec);
        const auto summary_stamp=std::filesystem::last_write_time(preview_file(current),ec);
        if(refresh || current!=root || stamp!=modified || summary_stamp!=summarised) {
            summary=read_save_preview(current); root=current; modified=stamp; summarised=summary_stamp;
        }
        next_check=now+std::chrono::seconds(1);
    }
    using namespace ui;
    if(width<=0 || height<=0) return;
    const float text=std::min(Layer::get().font_size()*0.85f,height/18.0f);
    const float gap=text*0.8f;
    const float card=(width-gap*2)/3;
    auto *draw=ImGui::GetBackgroundDrawList();
    for(int i=0;i<3;++i) {
        const float left=x+i*(card+gap);
        const ImVec2 end{left+card,y+height};
        draw->AddRectFilled({left,y},end,IM_COL32(12,22,29,226),text*0.5f);
        draw->AddRect({left,y},end,colors::kPanelEdge,text*0.5f);
        draw->PushClipRect({left+gap,y+gap},{end.x-gap,end.y-gap},true);
        const auto line=[&](float row, ImU32 color, const std::string &value) {
            draw->AddText(ImGui::GetFont(),text,{left+gap,y+gap+row*text*1.35f},color,value.c_str());
        };
        line(0,colors::kTextDim,host_text::format("launcher.save.slot",{std::to_string(i+1)}));
        const auto &slot=summary.slots[i];
        if(summary.status==PreviewStatus::Unavailable) {
            line(1.6f,colors::kText,host_text::get("launcher.save.unavailable"));
            line(3.1f,colors::kTextDim,host_text::get("launcher.save.unavailable_hint"));
        } else if(summary.status==PreviewStatus::Pending) {
            line(1.6f,colors::kText,host_text::get("launcher.save.pending"));
            line(3.1f,colors::kTextDim,host_text::get("launcher.save.pending_hint"));
        } else if(!slot.occupied) {
            line(1.6f,colors::kTextDim,host_text::get("launcher.save.empty"));
        } else {
            if(direct_load::enabled()) {
                const auto cursor=ImGui::GetCursorScreenPos();
                ImGui::PushID(i);
                ImGui::SetCursorScreenPos({left+gap,y+gap+text*1.6f});
                ImGui::PushFont(nullptr,text);
                ImGui::PushStyleColor(ImGuiCol_Button,colors::kRow);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,colors::kRowHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,colors::kRowFocus);
                ImGui::PushStyleColor(ImGuiCol_Text,colors::kAccentBright);
                ImGui::PushStyleColor(ImGuiCol_Border,colors::kAccent);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.0f);
                ImGui::BeginDisabled(slot.appearance_key.size()!=64);
                if(ImGui::Button(host_text::cstr("launcher.save.load"),{card-gap*2,text*1.8f})) {
                    // Re-read on click: imports/restores may have changed this card.
                    const auto fresh=read_save_preview(root);
                    if(fresh.status==PreviewStatus::Ready && fresh.slots[i].occupied &&
                       fresh.slots[i].name==slot.name && fresh.slots[i].appearance_key==slot.appearance_key)
                        direct_load::request(i,fresh.slots[i]);
                }
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(5);
                ImGui::PopFont();
                ImGui::PopID();
                ImGui::SetCursorScreenPos(cursor);
                }
            const float portrait_top=y+gap+text*(direct_load::enabled()?3.9f:1.8f);
            const float equipment_top=y+height-gap-text*9.0f;
            const float info_width=(card-gap*2)*.43f;
            const float portrait_height=std::max(0.0f,equipment_top-portrait_top-text*.6f);
            draw_save_portrait(i,slot,left+gap+info_width,portrait_top,card-gap*2-info_width,portrait_height);
            constexpr const char *labels[]={"launcher.gear.weapon","launcher.gear.helmet","launcher.gear.torso",
                "launcher.gear.braces","launcher.gear.waist","launcher.gear.feet"};
            const float row_height=(y+height-gap-equipment_top)/6;
            const float gear_label=std::min(text*.45f,row_height*.30f);
            const float gear_text=std::min(text*.90f,row_height*.58f);
            for(unsigned e=0;e<6;++e) {
                const float top=equipment_top+row_height*e;
                const float icon=std::min(text*1.2f,row_height*.75f);
                equipment::draw_icon(slot.equipment[e],left+gap,top+gear_label*.5f,icon);
                const float tx=left+gap+icon+gear_text*.35f;
                const float right=left+card-gap;
                draw->PushClipRect({tx,top},{right,top+row_height},true);
                draw->AddText(ImGui::GetFont(),gear_label,{tx,top},colors::kTextDim,host_text::cstr(labels[e]));
                const auto value=slot.details_available?equipment::name(slot.equipment[e]):std::string("—");
                draw->AddText(ImGui::GetFont(),gear_text,{tx,top+gear_label*1.2f},colors::kText,value.c_str());
                draw->PopClipRect();
            }
            const float info_text=std::min(text*.8f,info_width/10.5f);
            const auto detail=[&](float row,ImU32 color,const std::string &value) {
                const float top=portrait_top+row*portrait_height/6;
                draw->PushClipRect({left+gap,top},{left+gap+info_width-text*.2f,top+portrait_height/6},true);
                draw->AddText(ImGui::GetFont(),info_text,{left+gap,top+info_text*.2f},color,value.c_str());
                draw->PopClipRect();
            };
            detail(0,colors::kAccentBright,slot.name);
            detail(1,colors::kTextDim,host_text::format("launcher.save.hunter_rank",{std::to_string(slot.hunter_rank)}));
            detail(2,colors::kText,host_text::format("launcher.save.zenny",{std::to_string(slot.zenny)}));
            detail(3,colors::kText,host_text::format("launcher.save.pokke_points",{slot.details_available?std::to_string(slot.pokke_points):"—"}));
            detail(4,colors::kText,host_text::format("launcher.save.guild_points",{slot.details_available?std::to_string(slot.guild_points):"—"}));
            detail(5,colors::kText,host_text::format("launcher.save.play_time",{format_play_time(slot.played_seconds)}));
        }
        draw->PopClipRect();
    }
}
}
#else
namespace portablekit::fu { void draw_save_preview(float,float,float,float,bool) {} }
#endif
