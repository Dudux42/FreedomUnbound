#include "gathering_nodes.hpp"
#include "gathering_marker_layout.hpp"
#include "damage_events.hpp"
#include "mod_overlay.hpp"
#include "imgui.h"
#include "ui/layer.hpp"
#include "gpu/vulkan_renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
namespace portablekit::fu::gathering_nodes {
namespace {
bool fresh(Status status) {
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return status.kind != Kind::None && now >= status.timestamp && now - status.timestamp < 250000000;
}
void enable(const std::filesystem::path &) { set_enabled(true); damage_events::set_camera_enabled(true); }
void disable() { set_enabled(false); damage_events::set_camera_enabled(false); }
bool visible() {
    if (ui::Layer::get().attached()) ui::Layer::get().renderer().set_overlay_projection_enabled(true);
    return fresh(snapshot());
}
void draw() {
    const auto status = snapshot();
    const auto camera = damage_events::camera();
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!fresh(status) || !camera.valid || now - static_cast<double>(camera.monotonic_ns) / 1e9 >= .25) return;
    const auto point = ui::Layer::get().renderer().project_overlay_world(status.head, &camera.view);
    if (!point) return;
    const auto *vp = ImGui::GetMainViewport();
    if (std::getenv("MHFU_GATHERING_PREVIEW"))
        ImGui::GetForegroundDrawList()->AddText({vp->Pos.x + 16, vp->Pos.y + vp->Size.y - 36},
            IM_COL32(255, 220, 140, 255), "Gathering preview - sample marker above hunter");
    const float r = std::clamp(vp->Size.y * (25.0f / 544.0f), 16.0f, 64.0f);
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
void register_renderer() {
    static const bool registered = mod_overlay::register_provider(
        {"gathering-nodes", {"gathering_nodes", "gathering_repeat"}, enable, disable, visible, draw});
    (void)registered;
}
}
