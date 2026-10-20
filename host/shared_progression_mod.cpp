#include "shared_progression.hpp"
#include "mod_overlay.hpp"
#include <cstdio>
namespace portablekit::fu::shared_progression {
namespace {
void enable(const std::filesystem::path &) {
    set_enabled(true);
    std::puts("[shared-progression] enabled");
    std::fflush(stdout);
}
void disable() { set_enabled(false); }
bool visible() { return false; }
void draw() {}
}
void register_mod() {
    static const bool registered = mod_overlay::register_provider(
        {"shared-guild-progression", {"shared_guild_progression"}, enable, disable, visible, draw});
    (void)registered;
}
}
