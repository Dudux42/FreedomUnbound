#include "gathering_nodes.hpp"
#include "psprecomp/runtime.hpp"
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <limits>

namespace portablekit::fu::gathering_nodes {
namespace {
std::atomic<bool> enabled{};
std::mutex mutex;
Status current;
std::uint32_t repeated_node{};
bool repeat_ready{};
bool blocked_until_release{}; // A combat press must not restart gathering after sheathing.
bool range(std::uint32_t address, std::uint32_t length, unsigned alignment = 4) {
    return address >= 0x08800000u && address <= 0x0A000000u - length && !(address % alignment);
}
float value(const psprecomp::AotFastView &mem, std::uint32_t address) {
    return std::bit_cast<float>(mem.aot_load32(address));
}
// Resolve the current area's node list from the verified USA map descriptors.
// All pointers are resolved again on the guest thread; none cross into drawing.
std::uint32_t nodes(const psprecomp::AotFastView &mem, std::uint32_t hunter) {
    const auto state = mem.aot_load32(0x089CC558u);
    const auto map = mem.aot_load32(0x08A62D9Cu);
    if (!range(state, 0x6AF11u) || !range(map, 100)) return 0;
    const auto area = mem.aot_load8(state + 0x6AF10u);
    const auto mapping = 0x09CE4808u + area * 8u;
    const auto count = mem.aot_load32(mapping);
    const auto ids = mem.aot_load32(mapping + 4);
    if (!count || count > 256 || !range(ids, count * 2, 2)) return 0;
    const auto zone = mem.aot_load16(hunter + 0x29Au);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (mem.aot_load16(ids + index * 2) != zone) continue;
        const auto offsets = mem.aot_load32(map + 0x60u);
        const auto data = mem.aot_load32(map + 0x50u);
        if (!range(offsets, (index + 1) * 4) || !range(data, 4)) return 0;
        const auto offset = mem.aot_load32(offsets + index * 4);
        if (!offset || offset > 0x02000000u || !range(data + offset, 24)) return 0;
        return data + offset;
    }
    return 0;
}
}
void clear() { std::lock_guard lock(mutex); current = {}; blocked_until_release = false; repeated_node = 0; repeat_ready = false; }
void set_enabled(bool on) {
    std::lock_guard lock(mutex);
    enabled.store(on, std::memory_order_relaxed);
    current = {}; blocked_until_release = false; repeated_node = 0; repeat_ready = false;
}
Status snapshot() { std::lock_guard lock(mutex); return current; }
void update(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &mem) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    std::lock_guard lock(mutex);
    if (!enabled.load(std::memory_order_relaxed)) return;
    current = {};
    repeat_ready = false;
    // The HUD hook exists outside quests too; require the quest overlay signature.
    if (mem.aot_load32(0x08845CE4u) != 0x3C020894u ||
        mem.aot_load32(0x09A6F490u) != 0x27BDFFF0u) { blocked_until_release = false; return; }
    const bool circle = (mem.aot_load32(0x08A62D88u) & 0x2000u) != 0;
    if (!circle) { blocked_until_release = false; repeated_node = 0; repeat_ready = false; }
    const auto hunter = mem.aot_load32(0x099998F4u);
    if (!range(hunter, 0x696u)) return;
    // Persistent weapon-drawn flag: USA weapon routines set actor+0x410 bit 3
    // (e.g. 09A71B68). Live bow samples read 0x201 sheathed, 0x209 drawn.
    // The action group alone is insufficient: drawn melee idle also uses zero.
    const bool weapon_drawn = (mem.aot_load32(hunter + 0x410u) & 8u) != 0;
    const auto action_group = mem.aot_load8(hunter + 0x298u);
    static const bool trace = std::getenv("MHFU_GATHERING_TRACE") != nullptr;
    static unsigned previous_state = 0xFFFFFFFFu;
    const unsigned state_key = action_group | (weapon_drawn ? 0x100u : 0u);
    if (trace && previous_state != state_key) {
        previous_state = state_key;
        std::printf("[gathering-visibility] action-group=%u minor=%u weapon-drawn=%u flags=%08X\n",
                    action_group, mem.aot_load8(hunter + 0x299u), weapon_drawn, mem.aot_load32(hunter + 0x410u));
        std::fflush(stdout);
    }
    if (weapon_drawn) { blocked_until_release |= circle; repeated_node = 0; return; }
    if (action_group != 0) return;
    static const bool fixture = [] {
        const char *flag = std::getenv("MHFU_GATHERING_PREVIEW");
        return flag && flag[0] == '1' && flag[1] == '\0';
    }();
    if (fixture) {
        current.kind = Kind::Gather; current.tool = true;
        current.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (unsigned i = 0; i < 3; ++i) current.head[i] = value(mem, hunter + 0x40u + i * 4);
        current.head[1] += 140;
        return;
    }
    const auto active = mem.aot_load8(hunter + 0x325u);
    if (active > 1) return;
    auto list = nodes(mem, hunter);
    const float x = value(mem, hunter + 0x200u);
    const float y = value(mem, hunter + 0x204u);
    const float z = value(mem, hunter + 0x208u);
    static unsigned frames{};
    if (trace && (++frames % 120u == 1)) {
        std::printf("[gathering] hunter=%08X zone=%u position=%.1f,%.1f,%.1f nodes=%08X\n",
            hunter, mem.aot_load16(hunter + 0x29Au), x, y, z, list);
        for (unsigned i = 0; list && i < 32 && range(list + i * 24, 24); ++i) {
            const auto record = list + i * 24;
            if (value(mem, record) == -1) break;
            std::printf("[gathering-node] %.1f,%.1f,%.1f r=%.1f id=%u type=%u\n",
                value(mem, record), value(mem, record + 4), value(mem, record + 8),
                value(mem, record + 12), mem.aot_load16(record + 16), mem.aot_load16(record + 20));
        }
        std::fflush(stdout);
    }
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
    // Lists use 24-byte records and an x=-1 terminator. Bound damaged lists.
    std::uint32_t selected = 0;
    float nearest = std::numeric_limits<float>::max();
    for (unsigned i = 0; list && i < 256; ++i, list += 24) {
        if (!range(list, 24)) return;
        const float nx = value(mem, list), ny = value(mem, list + 4), nz = value(mem, list + 8);
        if (nx == -1.0f) break;
        const float radius = value(mem, list + 12);
        if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz) ||
            !std::isfinite(radius) || radius < 0 || radius > 10000) return;
        if (y < ny - 200 || y >= ny + 100 ||
            (x - nx) * (x - nx) + (z - nz) * (z - nz) > radius * radius) continue;
        // The game's harvest routine at 09AAAC10 reads +18 and returns
        // 0xFFFE when zero. It decrements/zeros this field at 09AAAD44..78.
        if (!mem.aot_load16(list + 18) || mem.aot_load16(list + 16) == 0xFFFFu ||
            mem.aot_load16(list + 20) >= 0x8000u) continue;
        const float distance = (x - nx) * (x - nx) + (z - nz) * (z - nz);
        if (distance < nearest) { nearest = distance; selected = list; }
    }
    if (selected) {
        const auto type = mem.aot_load16(selected + 20);
        current.kind = type == 3 ? Kind::Mine : type == 4 ? Kind::Bugs : Kind::Gather;
        std::uint16_t tool = 0;
        if (current.kind != Kind::Gather) {
            // Pouch entries are four bytes; original mod scans these 25 slots.
            for (std::uint32_t slot = 0; slot < 25; ++slot) {
                const auto item = mem.aot_load16(0x09A04C46u + slot * 4);
                const auto quantity = mem.aot_load16(0x09A04C48u + slot * 4);
                const bool matches = current.kind == Kind::Mine
                    ? (item >= 0x90 && item <= 0x92) || item == 0x4C9
                    : (item >= 0x94 && item <= 0x96) || item == 0x4CA;
                if (matches && quantity) { tool = item; break; }
            }
        }
        current.tool = current.kind == Kind::Gather || tool != 0;
        for (unsigned i = 0; i < 3; ++i) {
            current.head[i] = value(mem, hunter + 0x40u + i * 4);
            if (!std::isfinite(current.head[i])) { current = {}; return; }
        }
        current.head[1] += 140.0f;
        current.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        // Feed held Circle through the original interaction dispatcher. It
        // accepts crouched follow-up states as well as standing idle.
        const auto action = mem.aot_load8(hunter + 0x299u);
        if (circle && repeated_node && repeated_node != selected) blocked_until_release = true;
        repeat_ready = circle && !blocked_until_release && current.tool;
        if (repeat_ready) repeated_node = selected;
        // Keep the established tool shortcut. Ordinary gathering is entirely
        // native: never force its action or reset its animation counters.
        if (repeat_ready && tool && action == 0) {
            mem.aot_store16(hunter + 0x694u, tool);
            mem.aot_store8(hunter + 0x299u, current.kind == Kind::Mine ? 0x51u : 0x52u);
            mem.aot_store8(hunter + 0x1D5u, 0);
            mem.aot_store8(hunter + 0x1D6u, 0);
        }
        return;
    }
    // Depletion or leaving the original radius ends this held session. A
    // nearby overlapping node is not silently selected for automatic gathering.
    if (circle && repeated_node) blocked_until_release = true;
    repeated_node = 0;
}
void repeat_input(psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    std::lock_guard lock(mutex);
    if (!repeat_ready || !range(repeated_node, 24) ||
        mem.aot_load32(0x088D61FCu) != 0xAC850484u ||
        mem.aot_load32(0x09A6F490u) != 0x27BDFFF0u ||
        !(mem.aot_load32(0x08A62D88u) & 0x2000u)) return;
    const auto root = mem.aot_load32(0x089CBD04u), hunter = mem.aot_load32(0x099998F4u);
    const auto state = mem.aot_load32(0x089CC558u);
    if (ctx.gpr[4] != hunter || !range(root, 0xB00) || !range(hunter, 0x696) || !range(state, 0x430)) return;
    const auto phase = mem.aot_load8(state + 46);
    if ((phase != 2 && phase != 3) || mem.aot_load8(root + 0x961) ||
        (mem.aot_load32(hunter + 0x410) & 8u) || !mem.aot_load16(repeated_node + 18)) return;
    const float dx = value(mem, hunter + 0x200) - value(mem, repeated_node);
    const float dz = value(mem, hunter + 0x208) - value(mem, repeated_node + 8);
    const float y = value(mem, hunter + 0x204), ny = value(mem, repeated_node + 4);
    const float radius = value(mem, repeated_node + 12);
    if (!std::isfinite(dx) || !std::isfinite(dz) || !std::isfinite(y) || !std::isfinite(ny) ||
        !std::isfinite(radius) || radius < 0 || y < ny - 200 || y >= ny + 100 || dx*dx + dz*dz > radius*radius) return;
    // a1 is about to become this hunter's pressed mask (+0x484), before the
    // native menu/action filters. Global and camera/UI input stay untouched.
    ctx.gpr[5] |= 0x2000u;
}

}
