#include "gathering_nodes.hpp"
#include "gathering_marker_layout.hpp"
#include "psprecomp/runtime.hpp"
#include <bit>
#include <stdexcept>
using namespace portablekit::fu::gathering_nodes;
void check(bool condition) { if (!condition) throw std::runtime_error("gathering regression"); }
int main() {
    for (float height : {544.0f, 1080.0f, 2160.0f}) {
        const float width = height * (1920.0f / 1080.0f);
        const float radius = std::clamp(height * (25.0f / 544.0f), 16.0f, 64.0f);
        const auto badge = marker_position(width * .5f, height * .57f, width, height, radius);
        check(badge[0] == width * .5f);
        check(badge[1] + radius * 1.1f < height * .42f); // Entire badge above item banner.
        check(badge[1] + radius * 1.1f < height * .57f); // Clear of hunter's head.
        const auto side = marker_position(width * .2f, height * .7f, width, height, radius);
        check(side[1] > height * .42f); // Outside banner, follow the hunter normally.
    }
    psprecomp::Runtime runtime;
    auto &mem = runtime.memory();
    auto view = mem.aot_fast_view();
    psprecomp::AllegrexContext ctx{};
    constexpr auto hunter = 0x090B3580u, state = 0x09100000u, map = 0x09200000u;
    constexpr auto root = 0x09500000u;
    mem.store32(0x089CBD04, root); mem.store8(state + 46, 2); mem.store32(0x088D61FC, 0xAC850484);
    ctx.gpr[4] = hunter;
    auto input = [&] {
        ctx.gpr[5] = 0x10; mem.store32(0x08A62D8C, 0x40);
        repeat_input(ctx, view);
        // Only the local hunter's copy changes, never global/UI input.
        check(mem.load32(0x08A62D8C) == 0x40u);
        return ctx.gpr[5];
    };
    constexpr auto ids = 0x09300000u, offsets = 0x09300100u, data = 0x09400000u, node = data + 24;
    auto number = [&](auto address, float value) { mem.store32(address, std::bit_cast<std::uint32_t>(value)); };
    mem.store32(0x08845CE4, 0x3C020894); mem.store32(0x09A6F490, 0x27BDFFF0);
    mem.store32(0x099998F4, hunter); mem.store32(0x089CC558, state); mem.store32(0x08A62D9C, map);
    mem.store8(state + 0x6AF10, 2); mem.store16(hunter + 0x29A, 7);
    mem.store32(0x09CE4808 + 16, 1); mem.store32(0x09CE4808 + 20, ids); mem.store16(ids, 7);
    mem.store32(map + 0x60, offsets); mem.store32(map + 0x50, data); mem.store32(offsets, 24);
    number(hunter + 0x200, 10); number(hunter + 0x204, 20); number(hunter + 0x208, 30);
    number(node, 10); number(node + 4, 20); number(node + 8, 30); number(node + 12, 50);
    mem.store16(node + 16, 1); mem.store16(node + 18, 5); mem.store16(node + 20, 3); number(node + 24, -1);
    update(ctx, view); check(snapshot().kind == Kind::None);
    set_enabled(true); update(ctx, view); check(snapshot().kind == Kind::Mine && !snapshot().tool);
    mem.store16(0x09A04C46, 0x90); mem.store16(0x09A04C48, 1);
    update(ctx, view); check(snapshot().tool); check(mem.load8(hunter + 0x299) == 0);
    // Persistent drawn flag must hide even when melee idle uses group zero.
    mem.store8(hunter + 0x299, 0x20);
    mem.store32(0x08A62D88, 0x2000);
    mem.store32(hunter + 0x410, 0x209);
    for (unsigned group : {0u, 1u, 4u, 5u}) {
        mem.store8(hunter + 0x298, group); update(ctx, view);
        check(snapshot().kind == Kind::None);
        check(mem.load8(hunter + 0x299) == 0x20 && mem.load16(hunter + 0x694) == 0);
    }
    mem.store32(hunter + 0x410, 0x201);
    mem.store8(hunter + 0x298, 0); update(ctx, view);
    check(snapshot().kind == Kind::Mine && mem.load8(hunter + 0x299) == 0x20);
    // Holding Circle through sheathing must not replay the combat press.
    mem.store32(0x08A62D88, 0); mem.store8(hunter + 0x299, 0); update(ctx, view);
    mem.store32(0x08A62D88, 0x2000); update(ctx, view);
    check(mem.load8(hunter + 0x299) == 0x51 && mem.load16(hunter + 0x694) == 0x90);
    mem.store8(hunter + 0x1D5, 7); update(ctx, view);
    check(mem.load8(hunter + 0x299) == 0x51 && mem.load8(hunter + 0x1D5) == 7); // No animation restart.
    mem.store8(hunter + 0x299, 0); update(ctx, view); check(mem.load8(hunter + 0x299) == 0x51); // Held repeat.
    mem.store32(0x08A62D88, 0); mem.store8(hunter + 0x299, 0); update(ctx, view);
    check(mem.load8(hunter + 0x299) == 0); // Release stops.

    mem.store32(0x08A62D88, 0); update(ctx, view);
    mem.store16(node + 20, 4); update(ctx, view); check(snapshot().kind == Kind::Bugs && !snapshot().tool);
    mem.store16(0x09A04C46, 0x94); update(ctx, view); check(snapshot().tool);
    mem.store8(hunter + 0x299, 0);
    mem.store32(0x08A62D88, 0x2000); update(ctx, view); check(mem.load8(hunter + 0x299) == 0x52);
    mem.store16(0x09A04C48, 0); update(ctx, view); check(!snapshot().tool);
    mem.store16(node + 20, 1); update(ctx, view); check(snapshot().kind == Kind::Gather && snapshot().tool);
    number(hunter + 0x200, 1000); update(ctx, view); check(snapshot().kind == Kind::None);
    number(hunter + 0x200, 10); number(hunter + 0x204, 120); update(ctx, view); check(snapshot().kind == Kind::None);
    number(hunter + 0x204, -180); update(ctx, view); check(snapshot().kind == Kind::Gather);
    number(hunter + 0x204, -181); update(ctx, view); check(snapshot().kind == Kind::None);
    number(hunter + 0x204, 20); mem.store8(hunter + 0x325, 2); update(ctx, view); check(snapshot().kind == Kind::None);
    mem.store8(hunter + 0x325, 0); mem.store32(offsets, 0xFFFFFFFF); update(ctx, view); check(snapshot().kind == Kind::None);
    mem.store32(offsets, 24); mem.store32(0x09A6F490, 0); update(ctx, view); check(snapshot().kind == Kind::None);
    mem.store32(0x09A6F490, 0x27BDFFF0); update(ctx, view); check(snapshot().kind == Kind::Gather);
    mem.store16(node + 18, 0); update(ctx, view); check(snapshot().kind == Kind::None);
    mem.store16(node + 18, 2); update(ctx, view); check(snapshot().kind == Kind::Gather);
    // Overlapping radii: the closer usable spot wins, then depletion reveals
    // the remaining spot instead of keeping an icon for the exhausted one.
    number(node, 45); mem.store16(node + 20, 3);
    number(node + 24, 10); number(node + 28, 20); number(node + 32, 30);
    number(node + 36, 50); mem.store16(node + 40, 2); mem.store16(node + 42, 3);
    mem.store16(node + 44, 4); number(node + 48, -1);
    update(ctx, view); check(snapshot().kind == Kind::Bugs);
    mem.store16(node + 42, 0); update(ctx, view); check(snapshot().kind == Kind::Mine);
    mem.store16(node + 18, 0); update(ctx, view); check(snapshot().kind == Kind::None);
    // A held input repeats all node kinds, waits for idle, and stops on depletion.
    number(node, 10); number(node + 24, -1);
    mem.store16(0x09A04C46, 0x90); mem.store16(0x09A04C48, 10);
    mem.store16(0x09A04C4A, 0x94); mem.store16(0x09A04C4C, 10);
    for (unsigned type : {0u, 3u, 4u}) {
        mem.store16(node + 20, type); mem.store16(node + 18, 3);
        mem.store32(0x08A62D88, 0); update(ctx, view);
        mem.store32(0x08A62D88, 0x2000); mem.store8(hunter + 0x299, 0);
        const unsigned expected = type == 3 ? 0x51 : type == 4 ? 0x52 : 0x47;
        for (unsigned remaining : {3u, 2u, 1u}) {
            mem.store16(node + 18, remaining); mem.store8(hunter + 0x299, 0); update(ctx, view);
            check(input() == 0x2010);
            check(mem.load8(hunter + 0x299) == (type == 0 ? 0u : expected));
        }
        mem.store16(node + 18, 0); mem.store8(hunter + 0x299, 0); update(ctx, view);
        check(snapshot().kind == Kind::None && mem.load8(hunter + 0x299) == 0);
    }
    // Ordinary crouched follow-up must receive fresh native Circle without
    // forcing standing idle or resetting the current animation.
    mem.store16(node + 20, 0); mem.store16(node + 18, 3);
    mem.store32(0x08A62D88, 0); update(ctx, view);
    mem.store32(0x08A62D88, 0x2000);
    for (unsigned action : {0x47u, 0x5Du, 0x48u}) {
        mem.store8(hunter + 0x299, action); mem.store8(hunter + 0x1D5, 9); update(ctx, view);
        check(input() == 0x2010 && mem.load8(hunter + 0x299) == action && mem.load8(hunter + 0x1D5) == 9);
    }
    // Input rechecks live state, including changes since the last HUD scan.
    mem.store8(root + 0x961, 1); check(input() == 0x10);
    mem.store8(root + 0x961, 0);
    mem.store8(state + 46, 4); check(input() == 0x10);
    mem.store8(state + 46, 2);
    mem.store32(hunter + 0x410, 0x209); check(input() == 0x10);
    mem.store32(hunter + 0x410, 0x201);
    number(hunter + 0x200, 1000); check(input() == 0x10);
    number(hunter + 0x200, 10); check(input() == 0x2010);
    ctx.gpr[4] = hunter + 0x2000; check(input() == 0x10);
    ctx.gpr[4] = hunter;
    mem.store32(0x088D61FC, 0); check(input() == 0x10);
    mem.store32(0x088D61FC, 0xAC850484);
    mem.store32(0x08A62D88, 0); check(input() == 0x10);
    mem.store32(0x08A62D88, 0x2000); mem.store16(node + 18, 0); check(input() == 0x10);
    // Leaving a spot cancels the held session, even if the hunter returns.
    mem.store16(node + 20, 0); mem.store16(node + 18, 3);
    mem.store32(0x08A62D88, 0); update(ctx, view);
    mem.store32(0x08A62D88, 0x2000); mem.store8(hunter + 0x299, 0); update(ctx, view);
    check(input() == 0x2010 && mem.load8(hunter + 0x299) == 0);
    number(hunter + 0x200, 1000); update(ctx, view);
    number(hunter + 0x200, 10); mem.store8(hunter + 0x299, 0); update(ctx, view);
    check(mem.load8(hunter + 0x299) == 0);
    clear(); check(snapshot().kind == Kind::None);
    set_enabled(false); update(ctx, view); check(snapshot().kind == Kind::None);
}
