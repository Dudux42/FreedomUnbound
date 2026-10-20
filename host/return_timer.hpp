#pragma once
#include <cstdint>
namespace psprecomp { class AotFastView; }
namespace portablekit::fu::return_timer {
struct Snapshot { unsigned frames{}, seconds{}; std::uint64_t timestamp{}; };
unsigned seconds_left(unsigned frames);
void set_enabled(bool);
void refresh(const psprecomp::AotFastView &);
Snapshot snapshot();
void register_renderer();
}
