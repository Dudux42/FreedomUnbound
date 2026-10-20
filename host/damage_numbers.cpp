#include "damage_numbers.hpp"
#include "damage_number_layout.hpp"
#include "damage_events.hpp"
#include "mod_overlay.hpp"
#include "launcher_ini.hpp"
#include "imgui.h"
#include "ui/layer.hpp"
#include "gpu/vulkan_renderer.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <charconv>
#include <string>

namespace portablekit::fu::damage_numbers {
namespace {
Animation animation;
std::uint64_t generation{};
bool preview_mode{};
bool world_preview{};
double preview_last{};
struct Options {
    float size=1.f, outline=1.f;
    float colors[3][3]{{1.f,190/255.f,120/255.f},{1.f,190/255.f,120/255.f},{1.f,82/255.f,82/255.f}};
};
Options options;
bool options_dirty{},options_failed{};
constexpr const char *color_keys[]{"damage_numbers.normal", "damage_numbers.critical", "damage_numbers.negative"};
void load_options() {
    static const bool loaded=[] {
        const auto read=[](const char *key,float &value,float low,float high) {
            if(const auto text=launcher_ini::get(key)) {
                float parsed{};const auto result=std::from_chars(text->data(),text->data()+text->size(),parsed);
                if(result.ec==std::errc{} && result.ptr==text->data()+text->size() && std::isfinite(parsed))value=std::clamp(parsed,low,high);
            }
        };
        read("damage_numbers.size",options.size,.75f,2.f);
        read("damage_numbers.outline",options.outline,0.f,3.f);
        for(unsigned i=0;i<3;i++)if(const auto text=launcher_ini::get(color_keys[i]);text && text->size()==6) {
            unsigned rgb{};const auto result=std::from_chars(text->data(),text->data()+6,rgb,16);
            if(result.ec==std::errc{} && result.ptr==text->data()+6)
                for(unsigned j=0;j<3;j++)options.colors[i][j]=float((rgb>>(16-j*8))&255)/255.f;
        }
        return true;
    }();(void)loaded;
}
bool save_options() {
    bool saved=launcher_ini::set("damage_numbers.size",std::to_string(options.size));
    saved=launcher_ini::set("damage_numbers.outline",std::to_string(options.outline)) && saved;
    for(unsigned i=0;i<3;i++) {
        char hex[7];std::snprintf(hex,sizeof(hex),"%02X%02X%02X",int(std::lround(options.colors[i][0]*255)),int(std::lround(options.colors[i][1]*255)),int(std::lround(options.colors[i][2]*255)));
        saved=launcher_ini::set(color_keys[i],hex) && saved;
    }
    return saved;
}
void settings() {
    load_options();auto &dirty=options_dirty;auto &failed=options_failed;
    dirty=ImGui::SliderFloat("Number size",&options.size,.75f,2.f,"%.2fx") || dirty;
    constexpr const char *labels[]{"Normal hits", "Critical hits", "Negative-affinity hits"};
    for(unsigned i=0;i<3;i++)dirty=ImGui::ColorEdit3(labels[i],options.colors[i],ImGuiColorEditFlags_NoInputs) || dirty;
    dirty=ImGui::SliderFloat("Outline strength",&options.outline,0.f,3.f,"%.2fx") || dirty;
    ImGui::TextDisabled("White outline; 0 turns it off. Critical hits always keep their *.");
    if(ImGui::Button("Restore defaults")){options=Options{};dirty=true;}
    if(failed){ImGui::TextWrapped("Could not save all settings. Changes apply until you close the launcher.");if(ImGui::Button("Retry saving"))dirty=true;}
    // Apply live, but wait until a drag ends before writing launcher.ini.
    if(dirty && !ImGui::IsAnyItemActive()){failed=!save_options();dirty=false;}
}
double seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void enable(const std::filesystem::path &) {
    load_options();
    animation.clear();
    damage_events::set_enabled(true);
    generation = damage_events::generation();
    if (ui::Layer::get().attached()) ui::Layer::get().renderer().set_overlay_projection_enabled(true);
}
void disable() {
    if(options_dirty){options_failed=!save_options();options_dirty=false;}
    damage_events::set_enabled(false);
    animation.clear();
    if (ui::Layer::get().attached()) ui::Layer::get().renderer().set_overlay_projection_enabled(false);
}
bool visible() {
    // Also finish saving when F9 is closed immediately after an edit.
    if(options_dirty && !ImGui::IsAnyItemActive()){options_failed=!save_options();options_dirty=false;}
    if (ui::Layer::get().attached()) ui::Layer::get().renderer().set_overlay_projection_enabled(true);
    const double now = seconds();
    if (generation != damage_events::generation()) {
        animation.clear();
        generation = damage_events::generation();
    }
    animation.expire(now);
    const auto batch = damage_events::drain();
    for (std::size_t i = 0; i < batch.count; ++i) {
        const auto &event = batch.events[i];
        const double born = static_cast<double>(event.monotonic_ns) / 1e9;
        if (event.positioned && now >= born && now - born < lifetime)
            animation.add_world(event.hp_loss, born, event.impact, event.affinity);
    }
    animation.expire(now);
    if (preview_mode && !world_preview && now - preview_last > 0.95) {
        preview_last = now;
        animation.clear();
        animation.add(7, now, 0.25f);
        animation.add(42, now, 0.50f, 0.46f, Affinity::Positive);
        animation.add(318, now, 0.75f, 0.46f, Affinity::Negative);
    }
    return animation.size() != 0;
}
void draw() {
    const auto *viewport = ImGui::GetMainViewport();
    auto *list = ImGui::GetForegroundDrawList();
    auto *font = ImGui::GetFont();
    const float base = std::clamp(viewport->Size.y * (22.0f / 544.0f), 16.0f, 72.0f) * options.size;
    const float outline = std::clamp(base / 44.0f, .5f, 1.5f);
    const double now = seconds();
    if (preview_mode)
        list->AddText({viewport->Pos.x + 16.0f, viewport->Pos.y + viewport->Size.y - 36.0f},
                      IM_COL32(255, 190, 120, 255), world_preview ? "World-position preview - sample value above hunter"
                                                                            : "Damage renderer preview - normal / critical / negative affinity");
    list->PushClipRect(viewport->Pos, {viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y}, true);
    const auto camera = damage_events::camera();
    const bool camera_ready = camera.valid && now - static_cast<double>(camera.monotonic_ns) / 1e9 < 0.25;
    for (const auto &number : animation) {
        const auto p = pose(number, now);
        float x = p.x, y = p.y;
        if (number.positioned) {
            if (!camera_ready) continue;
            const auto projected = ui::Layer::get().renderer().project_overlay_world(number.world, &camera.view);
            if (!projected) continue;
            x = (*projected)[0];
            // Offset in screen space so the glyph sits above the contact,
            // even when the camera looks down or the target is very close.
            y = (*projected)[1] + (p.y - number.y) - base * 1.2f / viewport->Size.y;
        }
        char text[16];
        std::snprintf(text, sizeof(text), "%d", number.amount);
        const float size = base * p.scale;
        const auto extent = font->CalcTextSizeA(size, 10000.0f, 0.0f, text);
        const bool critical = number.affinity == Affinity::Positive;
        const float star_size = size * 0.65f;
        const float star_width = critical ? font->CalcTextSizeA(star_size, 10000.0f, 0.0f, "*").x : 0.0f;
        const ImVec2 at{viewport->Pos.x + viewport->Size.x * x - (extent.x + star_width) * 0.5f,
                        viewport->Pos.y + viewport->Size.y * y - extent.y * 0.5f};
        const int alpha = static_cast<int>(p.alpha * 255.0f);
        const auto shadow = IM_COL32(255, 255, 255, alpha);
        const auto &rgb = options.colors[number.affinity==Affinity::Negative?2:critical?1:0];
        const auto ink = IM_COL32(int(std::lround(rgb[0]*255)), int(std::lround(rgb[1]*255)), int(std::lround(rgb[2]*255)), alpha);
        const auto outlined = [&](const char *glyphs, float glyph_size, ImVec2 position) {
            const float edge = outline * options.outline + std::max(0.6f, glyph_size * .035f);
            const float diagonal = edge * .70710678f;
            for (const ImVec2 delta : {ImVec2{-edge, 0}, ImVec2{edge, 0},
                                       ImVec2{0, -edge}, ImVec2{0, edge},
                                       ImVec2{-diagonal, -diagonal}, ImVec2{diagonal, -diagonal},
                                       ImVec2{-diagonal, diagonal}, ImVec2{diagonal, diagonal}})
                if(options.outline>0)list->AddText(font, glyph_size, {position.x + delta.x, position.y + delta.y}, shadow, glyphs);
            // Thicken the colored glyph itself, keeping the white border outside.
            const float weight = std::max(.6f, glyph_size * .035f);
            for (const ImVec2 delta : {ImVec2{0, 0}, ImVec2{-weight, 0}, ImVec2{weight, 0},
                                       ImVec2{0, -weight}, ImVec2{0, weight}})
                list->AddText(font, glyph_size, {position.x + delta.x, position.y + delta.y}, ink, glyphs);
        };
        outlined(text, size, at);
        if (critical) outlined("*", star_size, {at.x + extent.x, at.y - size * 0.08f});
    }
    list->PopClipRect();
}
}
void register_renderer() {
    static const bool initialized = [] {
        const bool registered = mod_overlay::register_provider(
            {"damage-numbers", {"damage_events"}, enable, disable, visible, draw, false, settings});
        // Explicit test-only preview; never set by normal desktop launches.
        const char *preview = std::getenv("MHFU_DAMAGE_NUMBERS_PREVIEW");
        world_preview = std::getenv("MHFU_DAMAGE_WORLD_PREVIEW") != nullptr;
        if (registered && ((preview && preview[0] == '1' && preview[1] == '\0') || world_preview)) {
            preview_mode = true;
            mod_overlay::activate("damage-numbers", {});
        }
        return registered;
    }();
    (void)initialized;
}
}
