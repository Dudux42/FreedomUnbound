#pragma once
namespace psprecomp { struct AllegrexContext; class AotFastView; }
namespace portablekit::fu::shared_progression {
void set_enabled(bool on);
void completion_gate(psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void register_mod();
}
