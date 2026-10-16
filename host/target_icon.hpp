#pragma once
#include "imgui.h"
#include <initializer_list>
namespace portablekit::fu {
inline void draw_target_icon(ImDrawList *list, ImVec2 center, float radius) {
    list->AddCircleFilled(center, radius + 1.5f, IM_COL32(12, 18, 12, 240), 20);
    const auto color = IM_COL32(255, 224, 112, 255);
    list->AddCircle(center, radius * .65f, color, 20, 1.3f);
    list->AddCircleFilled(center, 1.1f, color, 8);
    for (const ImVec2 direction : {ImVec2{1,0}, ImVec2{-1,0}, ImVec2{0,1}, ImVec2{0,-1}})
        list->AddLine({center.x + direction.x * radius * .45f, center.y + direction.y * radius * .45f},
                      {center.x + direction.x * radius, center.y + direction.y * radius}, color, 1.3f);
}
}
