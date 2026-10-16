#pragma once
#include <algorithm>
#include <array>
namespace portablekit::fu::gathering_nodes {
// Input is the projected point at the hunter's approximate head height.
inline std::array<float, 2> marker_position(float x, float y, float width, float height, float radius) {
    const float gap = height * (22.0f / 544.0f);
    float cy = y - radius - gap;
    // Reserve the centered item-notification band (measured from F12 captures).
    // Leave padding for the complete hexagon and its outline.
    const float left = x - radius * 1.2f, right = x + radius;
    const float bottom = cy + radius * 1.1f;
    if (right > width * .30f && left < width * .70f && bottom > height * .42f)
        cy = std::min(cy, height * .42f - gap - radius * 1.1f);
    return {std::clamp(x, radius * 1.2f, width - radius), std::max(radius, cy)};
}
}
