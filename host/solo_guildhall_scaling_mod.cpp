#include "solo_guildhall_scaling.hpp"
#include "mod_overlay.hpp"
namespace portablekit::fu::solo_guildhall_scaling {
namespace {
void enable(const std::filesystem::path &) { set_enabled(true); }
void disable() { set_enabled(false); }
bool visible() { return false; }
void draw() {}
}
void register_mod() {
    static const bool registered=mod_overlay::register_provider(
        {"solo-guildhall-scaling", {"offline_hall_hp"}, enable, disable, visible, draw});
    (void)registered;
}
}
