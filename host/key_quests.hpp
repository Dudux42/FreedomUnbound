#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::key_quests {
struct Marker { float x{}, y{}; std::uint16_t quest{}; };
struct Snapshot { std::array<Marker, 24> markers{}; std::size_t count{}; };
bool is_key(const psprecomp::AotFastView &, unsigned quest);
void set_enabled(bool enabled);
bool visible();
Snapshot take_markers();
void register_renderer();
void capture(psprecomp::AllegrexContext &, const psprecomp::AotFastView &, unsigned point);
}
