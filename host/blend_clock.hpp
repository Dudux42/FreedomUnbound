#pragma once
#include "half_step.hpp"

namespace portablekit::fu {
// The legacy updater consumes a whole tick on the setup frame. Preserve that
// phase, then advance smoothly at half steps between its subsequent updates.
struct BlendClock {
    HalfStep step{1u};
    bool first{true};
    float weight_scale() const { return first ? 1.0f : 0.5f; }
    std::uint32_t tick() {
        first = false;
        return step.consume(1u);
    }
};
}
