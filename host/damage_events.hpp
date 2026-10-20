#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::damage_events {
enum class Affinity { Normal, Positive, Negative };
inline Affinity affinity_from_flags(std::uint8_t flags) {
    if (flags & 0x40u) return Affinity::Negative;
    return flags & 0x80u ? Affinity::Positive : Affinity::Normal;
}
// Natural HP loss before OneHit changes the pending store. No attacker attribution.
struct Event {
    std::uint32_t actor{};
    int hp_before{}, hp_after{}, hp_loss{};
    std::uint64_t sequence{};
    std::uint64_t monotonic_ns{};
    std::array<float, 3> impact{};
    Affinity affinity{};
    bool positioned{};
};
struct Batch { std::array<Event, 64> events{}; std::size_t count{}; };
struct Camera { std::array<float, 16> view{}; std::uint64_t monotonic_ns{}; bool valid{}; };
void capture_camera(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
Camera camera();
void set_camera_enabled(bool); // Independent world-marker consumer.
void set_enabled(bool enabled); // Disabling discards queued events.
void clear(); // Provider must also call on quest/area/session reset.
std::uint64_t generation(); // Changes on clear/enable/disable, invalidating animations.
Batch drain(); // Render thread receives values only, never dereferences actor.
void capture(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void begin_hit(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void commit_hit(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
}
