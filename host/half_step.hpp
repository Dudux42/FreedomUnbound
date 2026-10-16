#pragma once
#include <cstdint>

namespace portablekit::fu {
// Carry the fractional half rather than rounding every odd decrement down.
struct HalfStep {
    std::uint32_t remainder{};
    std::uint32_t consume(std::uint32_t amount) {
        const auto total = static_cast<std::uint64_t>(amount) + remainder;
        remainder = static_cast<std::uint32_t>(total & 1u);
        return static_cast<std::uint32_t>(total / 2u);
    }
};
}
