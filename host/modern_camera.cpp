#include "modern_camera.hpp"
#include "monster_catalog.hpp"
#include "camera/camera_input.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <mutex>
namespace portablekit::fu::modern_camera {
namespace {
std::atomic<bool> enabled{}, active{}, target_controls{};
std::atomic<unsigned> held_triggers{}, requests{};
std::mutex target_mutex;
Target published;
std::uint32_t selected_actor{};
unsigned selected_species{};
std::uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Orbit {
    std::uint32_t root{}, table{};
    unsigned area{}, preset{};
    float elevation{}, yaw_remainder{};
    bool initialized{};
} orbit;
bool valid(std::uint32_t p, unsigned bytes) {
    return !(p & 3u) && p >= 0x08800000u && p <= 0x0A000000u - bytes;
}
float number(const psprecomp::AotFastView &m, std::uint32_t p) {
    return std::bit_cast<float>(m.aot_load32(p));
}
std::uint32_t context(const psprecomp::AotFastView &m) {
    if (!enabled.load() || m.aot_load32(0x08887614u) != 0x86820080u ||
        m.aot_load32(0x08887834u) != 0xC6600200u ||
        m.aot_load32(0x09A6F490u) != 0x27BDFFF0u) return 0;
    const auto state = m.aot_load32(0x089CC558u), r = m.aot_load32(0x089CBD04u);
    if (!valid(state, 0x430) || !valid(r, 0xB00)) return 0;
    // Decimal 46 is the byte used by USA quest-state stores (not offset 0x46).
    const auto phase = m.aot_load8(state + 46);
    if ((phase != 2 && phase != 3) || m.aot_load8(r + 0xA8D) || m.aot_load8(r + 0x961) ||
        m.aot_load8(r + 0xB0 + 0x91) != 1) return 0;
    const auto hunter = m.aot_load32(r + 0xA84), c = r + 0x150;
    if (!valid(hunter, 0x700)) return 0;
    const auto table = m.aot_load32(c + 0x6C);
    const auto preset = m.aot_load8(r + 0x14);
    if (!valid(table, 0xC0) || preset > 4) return 0;
    const auto selected = table + (preset ? preset * 32 : 160);
    if (m.aot_load32(c + 0x70) != selected) return 0;
    return r;
}
struct Parameters { float height{}, distance{}, focus{}, angle{}; };
bool parameters(const psprecomp::AotFastView &m, std::uint32_t table, std::array<Parameters, 5> &p) {
    for (unsigned i = 0; i < p.size(); ++i) {
        const auto record = table + (i ? i * 32 : 160);
        auto &v = p[i];
        v.height = number(m, record + 4); v.distance = number(m, record + 8); v.focus = number(m, record + 16);
        if (!std::isfinite(v.height) || !std::isfinite(v.distance) || !std::isfinite(v.focus) ||
            std::abs(v.height) > 2000 || v.distance < 1 || v.distance > 3000 || std::abs(v.focus) > 2000) return false;
        v.angle = std::atan2(v.height - v.focus, v.distance) * (180.0f / 3.14159265f);
        if (i && v.angle >= p[i - 1].angle) return false;
    }
    return true;
}
void reset() {
    orbit = {}; camera::discard(); requests.store(0);
    selected_actor = 0; selected_species = 0;
    std::lock_guard lock(target_mutex); published = {};
}
struct Candidate { std::uint32_t actor{}; unsigned species{}; float x{}, y{}, z{}; double distance{}; };
Candidate select_target(const psprecomp::AotFastView &m, std::uint32_t hunter, unsigned area, bool cycle) {
    std::array<Candidate, 20> candidates{};
    unsigned count = 0;
    const auto manager = m.aot_load32(0x09C122B0u);
    const float hx = number(m, hunter + 0x200), hy = number(m, hunter + 0x204), hz = number(m, hunter + 0x208);
    if (valid(manager, 4724) && std::isfinite(hx) && std::isfinite(hy) && std::isfinite(hz)) {
        for (unsigned slot = 0; slot < 20; ++slot) {
            const auto actor = m.aot_load32(manager + 4640 + slot * 4);
            if (!valid(actor, 0x420) || actor == hunter || !m.aot_load32(actor) ||
                m.aot_load16(actor + 0x29A) != area) continue;
            const auto species = m.aot_load8(actor + 0x1E8);
            const int hp = static_cast<std::int16_t>(m.aot_load16(actor + 0x2E4));
            const auto maximum = m.aot_load16(actor + 0x41E);
            if (!monster_hp::large_monster_name(species) || hp <= 0 || !maximum || maximum > 32767 || hp > maximum) continue;
            if (std::any_of(candidates.begin(), candidates.begin() + count,
                [actor](const Candidate &v) { return v.actor == actor; })) continue;
            const float x = number(m, actor + 0x200), y = number(m, actor + 0x204), z = number(m, actor + 0x208);
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                std::abs(x) > 10000000 || std::abs(y) > 10000000 || std::abs(z) > 10000000) continue;
            const double dx = double(x) - hx, dy = double(y) - hy, dz = double(z) - hz;
            candidates[count++] = {actor, species, x, y, z, dx*dx + dy*dy + dz*dz};
        }
    }
    // Explicit developer fixture for native input/projection checks in an
    // empty camp. No fake actors enter the guest registry or receive writes.
    static const bool preview = [] {
        const auto *s = std::getenv("MHFU_TARGET_CAMERA_PREVIEW");
        return s && s[0] == '1' && !s[1];
    }();
    if (preview && std::isfinite(hx) && std::isfinite(hy) && std::isfinite(hz)) {
        count = 2;
        candidates[0] = {1, 1, hx + 1000, hy, hz, 1000000};
        candidates[1] = {2, 11, hx, hy, hz - 2000, 4000000};
    }
    std::sort(candidates.begin(), candidates.begin() + count, [](const Candidate &a, const Candidate &b) {
        return a.distance == b.distance ? a.actor < b.actor : a.distance < b.distance;
    });
    unsigned index = 0;
    for (; index < count; ++index)
        if (candidates[index].actor == selected_actor && candidates[index].species == selected_species) break;
    if (index == count) index = 0; // Acquire nearest; keep it stable until cycled or lost.
    else if (cycle && count > 1) index = (index + 1) % count;
    const Candidate result = count ? candidates[index] : Candidate{};
    static const bool trace = std::getenv("MHFU_TARGET_CAMERA_TRACE") != nullptr;
    if (trace && (selected_actor != result.actor || selected_species != result.species || cycle)) {
        std::printf("[target-camera] selected=%08X species=%u area=%u candidates=%u cycle=%u\n",
            result.actor, result.species, area, count, cycle);
        std::fflush(stdout);
    }
    selected_actor = result.actor; selected_species = result.species;
    { std::lock_guard lock(target_mutex); published = {result.actor, result.species, area, now_ns()}; }
    return result;
}
}
void set_enabled(bool on) { enabled.store(on); active.store(false); target_controls.store(false); held_triggers.store(3); reset(); }
Target target() {
    std::lock_guard lock(target_mutex);
    return now_ns() - published.timestamp < 500000000u ? published : Target{};
}
unsigned triggers(bool left, bool right, bool available) {
    if (!available) { held_triggers.store(3); requests.store(0); return 0; }
    const unsigned held = (left ? 1u : 0u) | (right ? 2u : 0u);
    const unsigned pressed = held & ~held_triggers.exchange(held);
    if (!enabled.load() || !target_controls.load()) return 0;
    if (active.load()) requests.fetch_or(pressed);
    return 3; // Reserve both extras, including when no target is present.
}
bool driving() { return active.load(); }
void prepare(const psprecomp::AotFastView &m, bool controls_enabled) {
    const auto state = m.aot_load32(0x089CC558u);
    const bool quest = enabled.load() && m.aot_load32(0x09A6F490u) == 0x27BDFFF0u && valid(state, 0x430) &&
        (m.aot_load8(state + 46) == 2 || m.aot_load8(state + 46) == 3);
    // Extras must not leak to PSP attacks/recenter while a quest's camera is
    // temporarily suspended (aiming, menus, special/scripted camera states).
    target_controls.store(quest);
    const bool supported = controls_enabled && context(m);
    active.store(supported);
    if (!supported) reset();
}
void yaw(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &m) {
    if (!driving()) return;
    const auto r = context(m), c = r + 0x150;
    if (!r || ctx.gpr[20] != c) { active.store(false); reset(); return; }
    std::array<Parameters, 5> p;
    const auto table = m.aot_load32(c + 0x6C);
    const auto preset = m.aot_load8(r + 0x14);
    if (!parameters(m, table, p)) { active.store(false); reset(); return; }
    const auto area = m.aot_load16(m.aot_load32(r + 0xA84) + 0x29A);
    if (!orbit.initialized || orbit.root != r || orbit.table != table || orbit.area != area) {
        selected_actor = 0; selected_species = 0;
        orbit = {r, table, area, preset, p[preset].angle, 0, true};
        camera::discard(); // Enter from the game's current camera, without queued scene motion.
    }
    if (orbit.preset != preset) { orbit.preset = preset; orbit.elevation = p[preset].angle; }
    const auto turn = camera::take();
    const auto request = requests.exchange(0);
    const auto hunter = m.aot_load32(r + 0xA84);
    const auto monster = select_target(m, hunter, area, (request & 2u) != 0);
    if ((request & 1u) && monster.actor) {
        const float dx = monster.x - number(m, hunter + 0x200);
        const float dz = monster.z - number(m, hunter + 0x208);
        if (std::hypot(dx, dz) > .01f) {
            // Native orbit offset rotates +Z around the hunter; face forward
            // toward the target by placing the camera on the opposite side.
            const auto angle = static_cast<std::uint16_t>(static_cast<int>(std::lround(
                std::atan2(-dx, -dz) * (65536.0f / (2 * 3.14159265f)))));
            m.aot_store16(c + 0x80, angle); m.aot_store16(c + 0x82, angle);
            const float focus = number(m, hunter + 0x204) + p[preset].focus;
            orbit.elevation = std::clamp(std::atan2(focus - (monster.y + 100), std::hypot(dx, dz)) *
                (180.0f / 3.14159265f), p.back().angle, p.front().angle);
            orbit.yaw_remainder = 0;
            if (std::getenv("MHFU_TARGET_CAMERA_TRACE")) {
                std::printf("[target-camera] snap=%08X yaw=%u elevation=%.3f\n", monster.actor, angle, orbit.elevation);
                std::fflush(stdout);
            }
        }
        return; // One snap, no follow, turn, attack, selection change or zoom.
    }
    // Physical D-pad and L keep their original actions and take priority.
    const auto held = m.aot_load16(c + 0x84);
    if (!(held & 0x1A0) && std::isfinite(turn.yaw_degrees)) {
        const float units = orbit.yaw_remainder - turn.yaw_degrees * (65536.0f / 360.0f);
        const int delta = static_cast<int>(std::clamp(units, -65536.0f, 65536.0f));
        orbit.yaw_remainder = units - delta;
        if (delta) {
            const auto target = static_cast<std::uint16_t>(m.aot_load16(c + 0x80) + delta);
            m.aot_store16(c + 0x80, target);
            // Apply the same delta to displayed yaw: responsive input without
            // changing the original recenter/collision smoothing difference.
            m.aot_store16(c + 0x82, static_cast<std::uint16_t>(m.aot_load16(c + 0x82) + delta));
        }
    } else orbit.yaw_remainder = 0;
    if (!(held & 0x150) && std::isfinite(turn.pitch_degrees))
        orbit.elevation = std::clamp(orbit.elevation + turn.pitch_degrees * .5f, p.back().angle, p.front().angle);
    static const bool trace = std::getenv("MHFU_MODERN_CAMERA_TRACE") != nullptr;
    if (trace && (turn.yaw_degrees || turn.pitch_degrees)) {
        std::printf("[modern-camera] area=%u yaw=%u elevation=%.3f request=%.3f,%.3f\n", area,
                    m.aot_load16(c + 0x80), orbit.elevation, turn.yaw_degrees, turn.pitch_degrees);
        std::fflush(stdout);
    }
}
void geometry(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &m) {
    if (!driving() || !orbit.initialized) return;
    const auto r = context(m), c = r + 0x150;
    if (!r || orbit.root != r || ctx.gpr[20] != c || !valid(ctx.gpr[29], 464)) return;
    const auto hunter = m.aot_load32(r + 0xA84);
    // These special hunter states select their own camera geometry. Preserve it.
    if (ctx.gpr[19] != hunter || m.aot_load8(hunter + 0x29C) != 0) return;
    std::array<Parameters, 5> p;
    if (!parameters(m, orbit.table, p)) return;
    unsigned lo = 0;
    while (lo < 3 && orbit.elevation < p[lo + 1].angle) ++lo;
    const float t = std::clamp((p[lo].angle - orbit.elevation) / (p[lo].angle - p[lo + 1].angle), 0.0f, 1.0f);
    const auto mix = [t](float a, float b) { return a + (b - a) * t; };
    const float height = mix(p[lo].height, p[lo + 1].height);
    const float distance = mix(p[lo].distance, p[lo + 1].distance);
    const float focus = number(m, hunter + 0x204) + mix(p[lo].focus, p[lo + 1].focus);
    if (!std::isfinite(focus)) return;
    // Original routine's temporary geometry, before trigonometry and native
    // environment queries. No shared preset table or instruction is modified.
    m.aot_store32(ctx.gpr[29] + 340, std::bit_cast<std::uint32_t>(height));
    m.aot_store32(ctx.gpr[29] + 344, std::bit_cast<std::uint32_t>(distance));
    m.aot_store32(c + 0x54, std::bit_cast<std::uint32_t>(focus));
}
}
