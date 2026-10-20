#pragma once
#include <array>
#include <cstdint>
namespace psprecomp { class AotFastView; }
namespace portablekit::fu::buff_icons {
enum class Kind : unsigned { Attack, Defense, Stamina, Recovery, Heat, Cold, Vision, MelodyBegin, Count=28 };
enum class Warning { Normal, Pulse, Urgent };
constexpr unsigned capacity=static_cast<unsigned>(Kind::Count);
constexpr unsigned item_count=static_cast<unsigned>(Kind::MelodyBegin),melody_count=21;
struct Buff { Kind kind{}; unsigned frames{}; };
struct Snapshot { std::array<Buff,capacity> buffs{}; unsigned count{}; std::uint64_t timestamp{}; };
struct Appearance { float scale{1}; bool red{}; };
Warning warning(unsigned frames);
Appearance appearance(unsigned frames,double time);
void set_enabled(bool);
void refresh(const psprecomp::AotFastView &);
Snapshot snapshot();
void register_renderer();
}
