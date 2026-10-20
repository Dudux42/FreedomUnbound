#include "half_step.hpp"
#include "blend_clock.hpp"
#include "message_clock.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

int main() {
    using portablekit::fu::HalfStep;
    HalfStep quest;
    std::uint64_t elapsed{};
    // A 50-minute hunt must still consume 90,000 legacy ticks at 60 updates/s.
    for (int frame = 0; frame < 50 * 60 * 60; ++frame) elapsed += quest.consume(1);
    if (elapsed != 90000u || quest.remainder != 0u) return 1;
    HalfStep a, b;
    if (a.consume(1) != 0u || b.consume(1) != 0u || a.consume(1) != 1u || b.consume(1) != 1u) return 2;
    HalfStep odd;
    if (odd.consume(3) != 1u || odd.consume(0) != 0u || odd.consume(3) != 2u) return 3;
    HalfStep maximum;
    const auto first = maximum.consume(UINT32_MAX), second = maximum.consume(UINT32_MAX);
    if (static_cast<std::uint64_t>(first) + second != UINT32_MAX) return 4;
    HalfStep fade;
    for (const auto duration : {1u, 16u, 13u, 3u, 1u, 16u}) {
        fade = {};
        std::uint32_t ticks{};
        for (std::uint32_t frame = 0; frame < duration * 2u; ++frame) {
            ticks += fade.consume(1u);
            if (frame + 1u < duration * 2u && ticks >= duration) return 5;
        }
        if (ticks != duration || fade.remainder != 0u) return 6;
    }
    // Interrupted transitions must discard a pending half tick, independently
    // of another channel. Completed odd-duration transitions end with no carry.
    HalfStep blend_a, blend_b;
    blend_a.consume(1u);
    blend_b.consume(1u);
    blend_a = {};
    if (blend_a.consume(1u) != 0u || blend_b.consume(1u) != 1u) return 7;
    for (auto duration : {3u, 5u, 9u}) {
        HalfStep blend;
        auto remaining = duration;
        for (auto frame = 0u; frame < duration * 2u; ++frame) {
            remaining -= blend.consume(1u);
            if (frame + 1u < duration * 2u && remaining == 0u) return 8;
        }
        if (remaining != 0u || blend.remainder != 0u) return 9;
    }
    // At each reference frame, both weight and completion must match. Native
    // half frames remain smooth; a one-tick blend still finishes immediately.
    using portablekit::fu::BlendClock;
    for (auto duration : {1u, 3u, 5u, 6u, 16u}) {
        BlendClock clock;
        auto remaining = duration;
        const float increment = 1.0f / duration;
        float weight = increment;
        float reference = increment;
        for (auto frame = 0u; frame < duration * 2u - 1u; ++frame) {
            weight += increment * clock.weight_scale();
            remaining -= clock.tick();
            if (frame % 2u == 0u) {
                reference += increment;
                if (std::fabs(reference - weight) > 1e-5f) return 10;
                if (remaining != duration - 1u - frame / 2u) return 11;
            }
            if (frame < duration * 2u - 2u && remaining == 0u) return 12;
        }
        if (remaining != 0u) return 13;
    }
    BlendClock restarted, other;
    restarted.tick(); other.tick();
    restarted.tick();
    restarted = {};
    if (restarted.weight_scale() != 1.0f || restarted.tick() != 1u || other.tick() != 0u) return 14;
    if (restarted.weight_scale() != 0.5f || restarted.tick() != 0u || other.tick() != 1u) return 15;
    using portablekit::fu::MessageClock;
    // Both complete and queue-interrupted holds leave cleanup taking two
    // updates per phase, regardless of a retained fractional hold tick.
    for (auto updates : {1u, 59u, 60u, 119u, 120u}) {
        MessageClock message;
        for (auto frame = 0u; frame < updates; ++frame) message.tick(1u);
        const auto carry = message.hold.remainder;
        // A pause invokes neither hold nor cleanup; resume retains the carry.
        if (message.cleanup.remainder != 0u) return 16;
        for (auto phase : {2u, 3u}) {
            if (message.tick(phase) != 0u || message.tick(phase) != 1u) return 17;
        }
        if (message.hold.remainder != carry) return 18;
        message = {};
        if (message.tick(1u) != 0u || message.tick(2u) != 0u) return 19;
    }
    std::puts("Native timing: countdown, blend, message cleanup, restarts and overflow checks passed");
    return 0;
}
