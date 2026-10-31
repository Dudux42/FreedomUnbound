#pragma once
// camera_aim: launcher hook for Camera Unbound smooth ranged aim (H2).
// Hook only: the provider (mod side) decides the step. Sites A (bow,
// 0x09BB6F8C) and B (bowgun, 0x09AA5248) in the USA game_task overlay
// (hash 36BD79C16BF70197). Mounted routes C/D are never hooked.
// The hook never reads, masks or synthesises pad or D-pad bits.
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::camera_aim {
struct AimStep { std::int32_t yaw_units; std::int32_t pitch_units; }; // 65536 yaw units per turn
enum class AimRoute : std::uint8_t { bow = 0, bowgun = 1, special_c = 2, cone_d = 3 };
using Provider = AimStep (*)(std::uint32_t hunter, AimRoute route, float dt);

inline constexpr std::uint32_t yaw_offset = 0x320, pitch_offset = 0x67E;
inline constexpr int pitch_min = -100, pitch_max = 100;
inline constexpr std::uint64_t aim_window_ms = 100;
struct Site { std::uint32_t address; std::uint32_t first; std::array<std::uint32_t, 4> words; };
// Words at the join and its neighbours, from the decoded overlay (HOOKS.md H2).
inline constexpr Site bow_site{0x09BB6F8Cu, 0x09BB6F84u, {0xA6230320u, 0x24030001u, 0x1203000Au, 0x02202021u}};
inline constexpr Site bowgun_site{0x09AA5248u, 0x09AA5244u, {0x02402021u, 0x240503EDu, 0x0E6A2D0Au, 0x240603EEu}};
// game_task identity: the probe camera.cpp already uses, plus 0x088D5EF0 (pitch helper).
inline constexpr std::uint32_t overlay_probe = 0x09A6F490u, overlay_word = 0x27BDFFF0u;
inline constexpr std::uint32_t pitch_helper = 0x088D5EF0u;
inline constexpr std::array<std::uint32_t, 3> pitch_helper_words{0x8083067Eu, 0x00651821u, 0xA083067Eu};

inline std::int8_t clamp_pitch(int current, std::int32_t delta) {
    const long long v = static_cast<long long>(current) + delta;
    return static_cast<std::int8_t>(std::clamp<long long>(v, pitch_min, pitch_max));
}
inline bool hunter_valid(std::uint32_t p) { return p >= 0x08800000u && p <= 0x0A000000u - 0x700u; }

// Per-site state. disarmed is sticky once a signature mismatch is seen.
struct SiteState { std::atomic<bool> disarmed{}; std::atomic<std::uint64_t> last_ms{}; };

enum class Result : std::uint8_t { disabled, disarmed, idle, applied };

// Core, templated on memory so it can be unit-tested without the runtime.
// M needs aot_load8/16/32 and aot_store8/16.
template <class M>
bool signature_ok(const M &m, const Site &s) {
    if (m.aot_load32(overlay_probe) != overlay_word) return false;
    for (unsigned i = 0; i < pitch_helper_words.size(); ++i)
        if (m.aot_load32(pitch_helper + 4 * i) != pitch_helper_words[i]) return false;
    for (unsigned i = 0; i < s.words.size(); ++i)
        if (m.aot_load32(s.first + 4 * i) != s.words[i]) return false;
    return true;
}
template <class M>
Result step(M &m, const Site &site, SiteState &state, AimRoute route, std::uint32_t hunter,
            Provider provider, bool enabled, std::uint64_t now_ms, std::atomic<std::uint64_t> &seen_ms) {
    if (!enabled || !provider) return Result::disabled;
    if (state.disarmed.load()) return Result::disarmed;
    if (!signature_ok(m, site)) { state.disarmed.store(true); return Result::disarmed; }
    if (route != AimRoute::bow && route != AimRoute::bowgun) return Result::disabled;
    if (!hunter_valid(hunter)) return Result::idle;
    // The join is reached on every gate-passing frame: that is "aiming".
    seen_ms.store(now_ms);
    const auto last = state.last_ms.exchange(now_ms);
    const auto elapsed = last && now_ms > last ? now_ms - last : 33u;
    const float dt = static_cast<float>(std::min<std::uint64_t>(elapsed, 100u)) / 1000.0f;
    const AimStep s = provider(hunter, route, dt);
    if (!s.yaw_units && !s.pitch_units) return Result::idle;
    if (s.yaw_units)  // u16 wrap, like the native step; never H+0x1F4.
        m.aot_store16(hunter + yaw_offset,
            static_cast<std::uint16_t>(m.aot_load16(hunter + yaw_offset) + static_cast<std::uint32_t>(s.yaw_units)));
    if (s.pitch_units) {  // direct s8 write, clamped, not through 0x088D5EF0
        const auto cur = static_cast<std::int8_t>(m.aot_load8(hunter + pitch_offset));
        m.aot_store8(hunter + pitch_offset, static_cast<std::uint8_t>(clamp_pitch(cur, s.pitch_units)));
    }
    return Result::applied;
}
inline bool recent(std::uint64_t seen, std::uint64_t now) { return seen && now >= seen && now - seen < aim_window_ms; }

// Runtime API.
void set_provider(Provider);   // null removes; the mod sets it when enabled
void set_enabled(bool);
bool armed();                  // capability present and no site disarmed
bool aiming();                 // the hook fired in the last 100 ms
void reset();                  // re-arm (new overlay load / tests)
// Called from the instrumented build copies, at the join, before its instruction.
void bow(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void bowgun(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
}
