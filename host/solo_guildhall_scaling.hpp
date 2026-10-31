#pragma once
#include <cstdint>
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::solo_guildhall_scaling {
void set_enabled(bool on);
bool eligible(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
std::uint32_t scaled_hp(std::uint32_t native_hp);
void register_mod();
}
