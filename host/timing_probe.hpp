#pragma once
#include <cstdint>

namespace psprecomp { class Runtime; class Elf32Image; struct AllegrexContext; }

namespace portablekit::fu {
// Initializes optional native research hooks, then installs a read-only observer.
void install_timing_probe(psprecomp::Runtime &, const psprecomp::Elf32Image &);
// Called only by optional diagnostic copies of generated units.
void sample_action_timing(std::uint32_t point, const psprecomp::AllegrexContext &);
}
