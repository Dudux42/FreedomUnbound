#pragma once
#include "half_step.hpp"
#include <cstdint>

namespace portablekit::fu {
// Queued messages can interrupt the hold between half ticks. Cleanup must
// start its own clock rather than inherit the interrupted hold's remainder.
struct MessageClock {
    HalfStep hold;
    HalfStep cleanup;
    std::uint32_t tick(std::uint8_t phase) {
        return (phase >= 2u ? cleanup : hold).consume(1u);
    }
};
}
