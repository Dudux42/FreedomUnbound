#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::monster_hp {
struct Buildup {unsigned current{},threshold{};bool valid{};};
struct Monster {
    std::uint32_t identity{}; // Opaque animation key; drawing never dereferences it.
    unsigned hp{}, maximum{};
    std::uint64_t timestamp{};
    unsigned species{}, area{};
    // Poison, paralysis, sleep, stun. Guest addresses never reach the UI.
    std::array<Buildup,4> status{};
};
struct Snapshot { std::array<Monster, 8> monsters{}; std::size_t count{}; bool quest{}; };
struct BreakEvent {unsigned species{},count{};};
bool low_hp(const Monster &monster);
void clear();
void set_enabled(bool);
void capture(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void refresh(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
Snapshot snapshot();
std::vector<BreakEvent> take_breaks();
void register_renderer();
}
