#pragma once
#include <cstdint>
#include <array>
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::gathering_nodes {
enum class Kind { None, Gather, Bugs, Mine, Carve };
struct Status { Kind kind{}; bool tool{}; std::uint64_t timestamp{}; std::array<float, 3> head{}; };
void set_enabled(bool);
void clear();
Status snapshot();
void update(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void repeat_input(psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void register_renderer();
}
