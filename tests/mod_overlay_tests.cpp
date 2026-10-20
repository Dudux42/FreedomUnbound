#include "mod_overlay.hpp"
#include "damage_events.hpp"
#include "psprecomp/runtime.hpp"
#include <stdexcept>
#include <bit>
using namespace portablekit::fu;
namespace {
void check(bool ok) { if (!ok) throw std::runtime_error("mod support regression"); }
int starts{}, stops{}, draws{}, settings_draws{};
bool show{};
void start(const std::filesystem::path &) { ++starts; }
void stop() { ++stops; }
bool visible() { return show; }
void draw() { ++draws; }
void settings() { ++settings_draws; }
}
int main() {
    check(!mod_overlay::supports("unknown"));
    check(!mod_overlay::has_settings("unknown"));mod_overlay::draw_settings("unknown");
    check(!mod_overlay::is_active("unknown"));
    check(!mod_overlay::register_provider({"incomplete"}));
    const mod_overlay::Provider provider{"test-overlay", {"damage_events"}, start, stop, visible, draw};
    check(mod_overlay::register_provider(provider));
    check(!mod_overlay::is_active(provider.id));
    check(!mod_overlay::register_provider(provider));
    check(mod_overlay::has_hook(provider.id, "overlay_draw"));
    check(mod_overlay::has_hook(provider.id, "damage_events"));
    check(!mod_overlay::has_hook(provider.id, "arbitrary_code"));
    check(!mod_overlay::visible());
    mod_overlay::activate(provider.id, {});
    check(mod_overlay::is_active(provider.id));
    check(starts == 1 && !mod_overlay::visible());
    show = true;
    check(mod_overlay::visible());
    mod_overlay::draw(); check(draws == 1);
    mod_overlay::activate(provider.id, {}); check(starts == 2 && stops == 1);
    mod_overlay::deactivate(provider.id); check(stops == 2 && !mod_overlay::visible());
    check(!mod_overlay::is_active(provider.id));
    mod_overlay::draw(); check(draws == 1);
    check(mod_overlay::register_provider({"persistent-overlay", {}, start, stop, visible, draw, true}));
    mod_overlay::activate("persistent-overlay", {});
    check(!mod_overlay::visible() && mod_overlay::visible(true));
    mod_overlay::draw(); check(draws == 1);
    mod_overlay::draw(true); check(draws == 2);
    mod_overlay::deactivate("persistent-overlay");
    check(!mod_overlay::visible(true));
    check(mod_overlay::register_provider({"configurable", {}, start, stop, visible, draw, false, settings}));
    check(mod_overlay::has_settings("configurable") && !mod_overlay::has_settings(provider.id));
    mod_overlay::draw_settings("configurable");check(settings_draws==1 && !mod_overlay::is_active("configurable"));

    psprecomp::Runtime runtime;
    auto &mem = runtime.memory();
    const auto view = mem.aot_fast_view();
    psprecomp::AllegrexContext ctx{};
    constexpr auto actor = 0x09100000u;
    ctx.gpr[21] = actor; ctx.gpr[2] = 1900;
    mem.store16(actor + 0x2E4u, 2000);
    mem.store32(0x09A6B154u, 0x106005F7u);
    mem.store32(0x09AC7AD4u, 0xA6A202E4u);
    mem.store32(0x09A85BACu, 0xA60202E4u);
    damage_events::capture(ctx, view); check(damage_events::drain().count == 0);
    damage_events::set_enabled(true);
    damage_events::capture(ctx, view);
    auto events = damage_events::drain();
    check(events.count == 1 && events.events[0].hp_loss == 100);
    check(mem.load16(actor + 0x2E4u) == 2000 && ctx.gpr[2] == 1900);
    check(damage_events::drain().count == 0);
    for (auto next : {2000u, 2100u}) { ctx.gpr[2] = next; damage_events::capture(ctx, view); }
    check(damage_events::drain().count == 0);
    ctx.gpr[2] = static_cast<unsigned>(-10);
    damage_events::capture(ctx, view);
    check(damage_events::drain().events[0].hp_loss == 2000); // Clamp overkill to remaining HP.
    ctx.gpr[2] = 1900;
    for (int i = 0; i < 100; ++i) damage_events::capture(ctx, view);
    events = damage_events::drain();
    check(events.count == 64 && events.events[63].sequence - events.events[0].sequence == 63);
    ctx.gpr[21] = 0x090B3580u; damage_events::capture(ctx, view);
    ctx.gpr[21] = 0xFFFFFFFFu; damage_events::capture(ctx, view);
    check(damage_events::drain().count == 0);
    ctx.gpr[21] = actor;
    mem.store32(0x09AC7AD4u, 0); damage_events::capture(ctx, view);
    check(damage_events::drain().count == 0);
    mem.store32(0x09AC7AD4u, 0xA6A202E4u);
    damage_events::capture(ctx, view); damage_events::clear(); check(damage_events::drain().count == 0);
    damage_events::capture(ctx, view); damage_events::set_enabled(false);
    check(damage_events::drain().count == 0);
    // Each accepted contact preserves its own position/affinity before the
    // gameplay task aggregates pending damage. No guest memory is changed.
    damage_events::set_enabled(true);
    mem.store32(0x09C43558u, 0x27BDFF70u);
    mem.store32(0x09C44D30u, 0x00511021u);
    constexpr auto stack = 0x09F00000u, attack = 0x09110000u, point = 0x09120000u;
    for (auto flags : {0u, 0x80u, 0x40u}) {
        ctx.gpr[29] = stack; ctx.gpr[4] = attack; ctx.gpr[6] = actor; ctx.gpr[9] = point;
        mem.store32(point, std::bit_cast<std::uint32_t>(10.0f));
        mem.store32(point + 4, std::bit_cast<std::uint32_t>(20.0f));
        mem.store32(point + 8, std::bit_cast<std::uint32_t>(30.0f));
        mem.store8(attack + 43, static_cast<std::uint8_t>(flags));
        damage_events::begin_hit(ctx, view);
        mem.store32(point, std::bit_cast<std::uint32_t>(999.0f));
        ctx.gpr[29] = stack - 144; ctx.gpr[19] = actor; ctx.gpr[21] = attack; ctx.gpr[17] = 42;
        damage_events::commit_hit(ctx, view);
        const auto batch = damage_events::drain();
        check(batch.count == 1 && batch.events[0].positioned && batch.events[0].impact[0] == 10.0f);
        check(batch.events[0].hp_loss == 42 && batch.events[0].monotonic_ns != 0);
        const auto expected = flags == 0x80u ? damage_events::Affinity::Positive :
                              flags == 0x40u ? damage_events::Affinity::Negative : damage_events::Affinity::Normal;
        check(batch.events[0].affinity == expected);
        damage_events::commit_hit(ctx, view); check(damage_events::drain().count == 0);
    }
    ctx.gpr[29] = stack; ctx.gpr[4] = attack; ctx.gpr[6] = actor; ctx.gpr[9] = point;
    damage_events::begin_hit(ctx, view);
    damage_events::clear();
    ctx.gpr[29] = stack - 144; ctx.gpr[19] = actor; ctx.gpr[21] = attack; ctx.gpr[17] = 42;
    damage_events::commit_hit(ctx, view); check(damage_events::drain().count == 0);
    damage_events::set_enabled(false);
    damage_events::set_enabled(true);
    mem.store32(0x0886941Cu, 0x03E00008u);
    const std::array<float, 16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    ctx.write_vfpu_matrix(identity.data(), 4, 4);
    damage_events::capture_camera(ctx, view);
    check(damage_events::camera().valid && damage_events::camera().view[15] == 1);
    auto affine = identity; affine[15] = 0; // Fourth VFPU row is scratch.
    ctx.write_vfpu_matrix(affine.data(), 4, 4);
    damage_events::capture_camera(ctx, view);
    check(damage_events::camera().valid && damage_events::camera().view[15] == 1);
    damage_events::clear(); check(!damage_events::camera().valid);
    damage_events::set_enabled(false);
    damage_events::set_camera_enabled(true);
    damage_events::capture_camera(ctx, view);
    check(damage_events::camera().valid && damage_events::drain().count == 0);
    damage_events::set_camera_enabled(false);
}
