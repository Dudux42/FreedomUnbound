#include "shared_progression.hpp"
#include "psprecomp/runtime.hpp"
#include <atomic>
#include <cstdio>
namespace portablekit::fu::shared_progression {
namespace {
std::atomic<bool> enabled{};
bool valid(unsigned p, unsigned size) {
    return !(p & 3u) && p >= 0x08800000u && p <= 0x0A000000u - size;
}
}
void set_enabled(bool on) { enabled.store(on); }
void completion_gate(psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &m) {
    // This callsite follows native success and urgent classification. Override
    // only a rejected poster check, never the global host/network-role helper.
    if (!enabled.load() || ctx.gpr[2] != 0 || ctx.gpr[31] != 0x088751B4u ||
        m.aot_load32(0x088751B4) != 0x1040001Bu ||
        m.aot_load32(0x0887CA08) != 0x3C0208A6u ||
        m.aot_load32(0x0884FE10) != 0x27BDFFF0u) return;
    const auto state = m.aot_load32(0x089CC558);
    if (!valid(state, 0x6AF18)) return;
    const auto phase = m.aot_load8(state + 46);
    if (phase != 3 && phase != 4) return; // No failed, abandoned or active quests.
    const auto quest = m.aot_load16(state + 0x6AF12);
    if (!quest || quest >= 4000) return; // Gathering Hall low/high/G quest IDs.
    const auto data = m.aot_load32(0x08A632C0);
    constexpr unsigned mode_offset = 0xF7A57C;
    if (!valid(data, mode_offset + 4) || !(m.aot_load32(data + mode_offset) & 1u)) return;
    const auto party = m.aot_load32(0x09A0B684);
    if (!valid(party, 0x12D8) || m.aot_load8(party + 0x12D6) != 0) return;
    // Recheck the game's own urgent table instead of shipping a quest list or
    // interpreting the reference cheat's release-specific progression masks.
    bool urgent = false, terminated = false;
    for (unsigned i = 0; i < 64; ++i) {
        const auto id = m.aot_load16(0x089A8684 + i * 2);
        if (id == 0xFFFFu) { terminated = true; break; }
        urgent |= id == quest;
    }
    if (!urgent || !terminated) return;
    ctx.gpr[2] = 1; // The original clear setter/unlock/save handling does the rest.
    std::printf("[shared-progression] participant clear credit quest=%u\n", quest);
    std::fflush(stdout);
}
}
