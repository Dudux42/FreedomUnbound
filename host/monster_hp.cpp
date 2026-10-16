#include "monster_hp.hpp"
#include "monster_catalog.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
namespace portablekit::fu::monster_hp {
namespace {
std::atomic<bool> enabled{};
std::mutex mutex;
std::array<Monster, 64> entries{};
Snapshot current;
std::uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool fresh(const Monster &entry, std::uint64_t now) {
    return entry.identity && now >= entry.timestamp && now - entry.timestamp < 500000000;
}
}
bool low_hp(const Monster &monster) {
    return monster.maximum && monster.hp && monster.hp <= monster.maximum / 4;
}
void clear() { std::lock_guard lock(mutex); entries = {}; current = {}; }
void set_enabled(bool on) {
    std::lock_guard lock(mutex);
    enabled.store(on, std::memory_order_relaxed);
    entries = {};
    current = {};
}
void capture(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    if (mem.aot_load32(0x09AC7AF4u) != 0x24020009u ||
        mem.aot_load32(0x09AC7AD4u) != 0xA6A202E4u) return;
    const auto actor = ctx.gpr[21];
    if ((actor & 3u) || actor < 0x08800000u || actor > 0x09FFFBDEu || actor == 0x090B3580u) return;
    // The USA routine reads these fields as signed halfwords. Reject corrupt
    // maxima instead of displaying wrapped negative health as a huge boss.
    const auto maximum = mem.aot_load16(actor + 0x41Eu);
    const int signed_hp = static_cast<std::int16_t>(mem.aot_load16(actor + 0x2E4u));
    const unsigned hp = signed_hp < 0 ? 0u : static_cast<unsigned>(signed_hp);
    const auto species = mem.aot_load8(actor + 0x1E8u);
    const auto area = mem.aot_load16(actor + 0x29Au);
    std::lock_guard lock(mutex);
    if (!enabled.load(std::memory_order_relaxed)) return;
    auto found = std::find_if(entries.begin(), entries.end(), [actor](const Monster &m) { return m.identity == actor; });
    if (!large_monster_name(species) || !maximum || maximum > 32767 || hp > maximum) {
        if (found != entries.end()) *found = {};
        return;
    }
    const auto now = now_ns();
    if (found == entries.end())
        found = std::find_if(entries.begin(), entries.end(), [now](const Monster &m) { return !fresh(m, now); });
    if (found == entries.end())
        found = std::min_element(entries.begin(), entries.end(), [](const Monster &a, const Monster &b) { return a.timestamp < b.timestamp; });
    static const bool trace = std::getenv("MHFU_MONSTER_HP_TRACE") != nullptr;
    if (trace && (found->identity != actor || found->hp != hp || found->maximum != maximum)) {
        std::printf("[monster-hp] actor=%08X hp=%u/%u\n", actor, hp, maximum);
        std::fflush(stdout);
    }
    *found = {actor, hp, maximum, now, species, area};
}
void refresh(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &mem) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    std::lock_guard lock(mutex);
    current = {};
    if (!enabled.load(std::memory_order_relaxed) || mem.aot_load32(0x09AC7AD4u) != 0xA6A202E4u) {
        entries = {}; return;
    }
    const auto state = mem.aot_load32(0x089CC558u);
    const auto valid = [](std::uint32_t p, std::uint32_t size) {
        return !(p & 3u) && p >= 0x08800000u && p <= 0x0A000000u - size;
    };
    if (!valid(state, 0x430u)) { entries = {}; return; }
    // USA quest states: 2 = hunting, 3 = success countdown. Rewards and
    // subsequent save screens use later states and must never retain bars.
    const auto phase = mem.aot_load8(state + 46u);
    if (phase != 2 && phase != 3) { entries = {}; return; }
    const auto hunter = mem.aot_load32(0x099998F4u);
    if (!valid(hunter, 0x1000u)) { entries = {}; return; }
    const auto area = mem.aot_load16(hunter + 0x29Au);
    const auto now = now_ns();
    entries = {};
    // USA spawn registration at 09AB168C writes manager + 4640 + slot*4;
    // its loop at 09AB16AC is bounded to 20 slots. The spawn callers load
    // that manager from 09C122B0. Read the registry, never scan arbitrary RAM.
    const auto manager = mem.aot_load32(0x09C122B0u);
    if (!valid(manager, 4724u)) return;
    for (unsigned slot = 0; slot < 20; ++slot) {
        const auto actor = mem.aot_load32(manager + 4640u + slot * 4u);
        if (!valid(actor, 0x420u) || actor == hunter || !mem.aot_load32(actor)) continue;
        const auto species = mem.aot_load8(actor + 0x1E8u);
        const auto maximum = mem.aot_load16(actor + 0x41Eu);
        const int hp = static_cast<std::int16_t>(mem.aot_load16(actor + 0x2E4u));
        if (!large_monster_name(species) || !maximum || maximum > 32767 || hp > static_cast<int>(maximum)) continue;
        // A slot may briefly alias another slot during a registry update.
        if (std::any_of(entries.begin(), entries.end(), [actor](const Monster &m) { return m.identity == actor; })) continue;
        entries[slot] = {actor, hp < 0 ? 0u : static_cast<unsigned>(hp), maximum,
                         now, species, mem.aot_load16(actor + 0x29Au)};
    }
    auto sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const Monster &a, const Monster &b) {
        return a.identity < b.identity;
    });
    for (const auto &entry : sorted) {
        if (!fresh(entry, now) || entry.area != area) continue;
        current.monsters[current.count++] = entry;
        if (current.count == current.monsters.size()) break;
    }
}
Snapshot snapshot() {
    std::lock_guard lock(mutex);
    return current;
}
}
