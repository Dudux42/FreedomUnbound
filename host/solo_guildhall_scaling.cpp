#include "solo_guildhall_scaling.hpp"
#include "monster_catalog.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <atomic>
namespace portablekit::fu::solo_guildhall_scaling {
namespace {
std::atomic<bool> enabled{};
bool valid(unsigned p, unsigned size) {
    return !(p & 3u) && p >= 0x08800000u && p <= 0x0A000000u-size;
}
}
void set_enabled(bool on) { enabled.store(on); }
std::uint32_t scaled_hp(std::uint32_t hp) {
    // Native HP is a positive signed halfword; preserve invalid/sentinel values.
    return hp && hp <= 32767 ? std::max(1u, (hp*65u+50u)/100u) : hp;
}
bool eligible(const psprecomp::AllegrexContext &c, const psprecomp::AotFastView &m) {
    // Stat selector zero is HP; other selectors include non-HP monster stats.
    if (!enabled.load() || c.gpr[5] != 0 ||
        m.aot_load32(0x09AC9120) != 0x908601E8u ||
        m.aot_load32(0x0887CA08) != 0x3C0208A6u) return false;
    const auto state=m.aot_load32(0x089CC558), data=m.aot_load32(0x08A632C0);
    const auto actor=c.gpr[4];
    if (!valid(state,0x6AF14) || !valid(data,0xF7A580) || !valid(actor,0x4C0)) return false;
    // The native online-mode bit is set by 088906A8 and cleared by 088906DC.
    // Never use the poster helper's boolean: it also accepts online hosts.
    if (m.aot_load32(data+0xF7A57C)&1u) return false;
    const auto quest=m.aot_load16(state+0x6AF12);
    if (quest<1000 || quest>=4000 || m.aot_load8(state+46)!=2) return false;
    return monster_hp::large_monster_name(m.aot_load8(actor+0x1E8)) != nullptr;
}
}
