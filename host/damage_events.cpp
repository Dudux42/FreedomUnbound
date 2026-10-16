#include "damage_events.hpp"
#include "psprecomp/runtime.hpp"
#include <atomic>
#include <chrono>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace portablekit::fu::damage_events {
namespace {
std::atomic<bool> enabled{};
std::atomic<bool> camera_enabled{};
std::mutex mutex;
Batch pending;
Camera current_camera;
std::uint64_t sequence{};
std::atomic<std::uint64_t> epoch{};
struct Contact {
    std::uint32_t stack{}, actor{}, attack{};
    std::uint64_t generation{};
    std::array<float, 3> position{};
    bool valid{};
};
// Guest execution owns these; render code receives copies only.
thread_local std::array<Contact, 16> contacts;
bool guest_range(std::uint32_t p, std::uint32_t bytes) {
    return !(p & 3u) && p >= 0x08800000u && p <= 0x0A000000u - bytes;
}
bool hit_overlay(const psprecomp::AotFastView &mem) {
    return mem.aot_load32(0x09C43558u) == 0x27BDFF70u &&
           mem.aot_load32(0x09C44D30u) == 0x00511021u;
}
}
void clear() { std::lock_guard lock(mutex); pending.count = 0; current_camera = {}; ++epoch; }
std::uint64_t generation() { return epoch.load(); }
void set_enabled(bool on) {
    std::lock_guard lock(mutex);
    enabled.store(on, std::memory_order_relaxed);
    pending.count = 0;
    current_camera = {};
    ++epoch;
}
Batch drain() {
    std::lock_guard lock(mutex);
    const auto result = pending;
    pending.count = 0;
    return result;
}
Camera camera() { std::lock_guard lock(mutex); return current_camera; }
void set_camera_enabled(bool on) { camera_enabled.store(on, std::memory_order_relaxed); }
void capture_camera(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    if ((!enabled.load(std::memory_order_relaxed) && !camera_enabled.load(std::memory_order_relaxed)) ||
        mem.aot_load32(0x0886941Cu) != 0x03E00008u) return;
    Camera next;
    // FU pre-applies its camera to object transforms and uploads an identity
    // GE view. The camera-update return holds world-to-view in M100. The
    // original mod copied it into M700 here for its later HUD draw.
    ctx.read_vfpu_matrix(next.view.data(), 4u, 4u);
    static const bool trace = std::getenv("MHFU_DAMAGE_TRACE") != nullptr;
    static std::uint64_t captures{};
    if (trace && (++captures & 63u) == 1u) {
        std::printf("[damage-camera-raw]");
        for (float v : next.view) std::printf(" %.6f", v);
        std::printf("\n"); std::fflush(stdout);
    }
    for (float v : next.view) if (!std::isfinite(v) || std::abs(v) > 1e7f) return;
    // The original transform uses only the three affine rows and explicitly
    // supplies homogeneous 1 after them; the fourth VFPU row is scratch.
    next.view[3] = next.view[7] = next.view[11] = 0.0f;
    next.view[15] = 1.0f;
    for (unsigned column = 0; column < 3; ++column) {
        float norm = 0;
        for (unsigned row = 0; row < 3; ++row) norm += next.view[column * 4 + row] * next.view[column * 4 + row];
        if (std::abs(norm - 1.0f) > 0.05f) return;
    }
    next.monotonic_ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    next.valid = true;
    std::lock_guard lock(mutex);
    if (enabled.load(std::memory_order_relaxed) || camera_enabled.load(std::memory_order_relaxed)) current_camera = next;
    static std::uint64_t frames{};
    if (trace && (++frames & 63u) == 1u) {
        std::printf("[damage-camera] view");
        for (float v : next.view) std::printf(" %.6f", v);
        std::printf("\n"); std::fflush(stdout);
    }
    // Isolated, labeled visual-test fixture, never enabled by a mod manifest.
    static const bool world_preview = std::getenv("MHFU_DAMAGE_WORLD_PREVIEW") != nullptr;
    static std::uint64_t last_preview{};
    if (world_preview && next.monotonic_ns - last_preview > 950000000u &&
        mem.aot_load16(0x090B3580u + 0x2E4u) > 0 && pending.count < pending.events.size()) {
        last_preview = next.monotonic_ns;
        Event event;
        event.hp_loss = 42; event.sequence = ++sequence; event.monotonic_ns = next.monotonic_ns;
        event.positioned = true; event.affinity = Affinity::Positive;
        for (unsigned i = 0; i < 3; ++i)
            event.impact[i] = std::bit_cast<float>(mem.aot_load32(0x090B3580u + 0x40u + i * 4));
        event.impact[1] += 100.0f;
        pending.events[pending.count++] = event;
    }
}
void begin_hit(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    if (!enabled.load(std::memory_order_relaxed) || !hit_overlay(mem)) return;
    const auto stack = ctx.gpr[29], actor = ctx.gpr[6], attack = ctx.gpr[4], point = ctx.gpr[9];
    Contact *slot = nullptr;
    for (auto &c : contacts) if (c.stack == stack) { slot = &c; break; }
    if (!slot) for (auto &c : contacts) if (!c.valid) { slot = &c; break; }
    if (!slot) slot = &contacts[0];
    *slot = {}; slot->stack = stack;
    if (!guest_range(stack, 16) || stack < 0x08800090u || !guest_range(actor, 0x500) ||
        actor == 0x090B3580u || !guest_range(attack, 0x80) || !guest_range(point, 12)) return;
    for (unsigned i = 0; i < 3; ++i) {
        slot->position[i] = std::bit_cast<float>(mem.aot_load32(point + 4u * i));
        if (!std::isfinite(slot->position[i]) || std::abs(slot->position[i]) > 1e7f) return;
    }
    slot->actor = actor; slot->attack = attack; slot->generation = epoch.load(); slot->valid = true;
}
void commit_hit(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    if (!enabled.load(std::memory_order_relaxed) || !hit_overlay(mem)) return;
    for (auto &c : contacts) {
        if (!c.valid || c.stack != ctx.gpr[29] + 144u) continue;
        c.valid = false;
        if (c.generation != epoch.load() || c.actor != ctx.gpr[19] || c.attack != ctx.gpr[21]) return;
        const int amount = static_cast<std::int16_t>(ctx.gpr[17]);
        if (amount <= 0 || static_cast<std::int16_t>(mem.aot_load16(c.actor + 0x2E4u)) <= 0) return;
        const auto affinity = affinity_from_flags(mem.aot_load8(c.attack + 0x2Bu));
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        std::lock_guard lock(mutex);
        if (!enabled.load(std::memory_order_relaxed) || c.generation != epoch.load()) return;
        if (pending.count == pending.events.size()) {
            std::move(pending.events.begin() + 1, pending.events.end(), pending.events.begin()); --pending.count;
        }
        Event event;
        event.actor = c.actor; event.hp_loss = amount; event.sequence = ++sequence;
        event.monotonic_ns = static_cast<std::uint64_t>(ns);
        event.impact = c.position; event.affinity = affinity; event.positioned = true;
        pending.events[pending.count++] = event;
        static const bool trace = std::getenv("MHFU_DAMAGE_TRACE") != nullptr;
        if (trace) {
            std::printf("[damage-hit] actor=%08x damage=%d affinity=%d impact=(%.3f,%.3f,%.3f)\n",
                        event.actor, amount, static_cast<int>(affinity), c.position[0], c.position[1], c.position[2]);
            std::fflush(stdout);
        }
        return;
    }
}
void capture(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    // Same content guards as the existing supported USA gameplay hooks.
    if (mem.aot_load32(0x09A6B154u) != 0x106005F7u ||
        mem.aot_load32(0x09AC7AD4u) != 0xA6A202E4u ||
        mem.aot_load32(0x09A85BACu) != 0xA60202E4u) return;
    const auto actor = ctx.gpr[21];
    if (actor == 0x090B3580u || actor < 0x08800000u || actor > 0x09FFF000u || (actor & 3u)) return;
    const int before = static_cast<std::int16_t>(mem.aot_load16(actor + 0x2E4u));
    const int after = static_cast<std::int16_t>(ctx.gpr[2]);
    if (before <= 0 || after >= before) return;
    std::lock_guard lock(mutex);
    if (!enabled.load(std::memory_order_relaxed)) return;
    // Drop the oldest under pressure, keeping a bounded amount of recent work.
    if (pending.count == pending.events.size()) {
        for (std::size_t i = 1; i < pending.count; ++i) pending.events[i - 1] = pending.events[i];
        --pending.count;
    }
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    pending.events[pending.count++] = {actor, before, after, before - (after < 0 ? 0 : after), ++sequence,
                                      static_cast<std::uint64_t>(ns)};
}
}
