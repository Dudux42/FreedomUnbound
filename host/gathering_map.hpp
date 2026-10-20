#pragma once
#include <cstdint>
#include <vector>
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::gathering_map {
enum class Kind { Gather, Mine, Bugs, Carve, Fishing };
unsigned filters();
void set_filters(unsigned mask);
struct Marker {float x{},y{};Kind kind{};};
struct View {float left{},top{},right{},bottom{};std::vector<Marker> markers;std::uint64_t timestamp{};};
void set_enabled(bool);
View snapshot();
void capture(const psprecomp::AllegrexContext &,const psprecomp::AotFastView &,bool zoomed);
void register_renderer();
}
