#include "monster_hp.hpp"
#include "monster_catalog.hpp"
#include "psprecomp/runtime.hpp"
#include <stdexcept>
#include <string>
#include <source_location>
#include <cstdio>
using namespace portablekit::fu::monster_hp;
void check(bool ok, const std::source_location at = std::source_location::current()) { if (!ok) { std::fprintf(stderr, "monster HP regression at line %u\n", at.line()); std::fflush(stderr); throw std::runtime_error("monster HP regression"); } }
int main() {
    psprecomp::Runtime runtime;
    auto &mem = runtime.memory(); auto view = mem.aot_fast_view();
    psprecomp::AllegrexContext ctx{};
    constexpr auto hunter = 0x090B3580u;
    mem.store32(0x09AC7AF4, 0x24020009); mem.store32(0x09AC7AD4, 0xA6A202E4);
    mem.store32(0x099998F4, hunter); mem.store16(hunter + 0x29A, 7);
    constexpr auto state = 0x09500000u, manager = 0x09600000u;
    mem.store32(0x089CC558, state); mem.store8(state + 46, 2);
    mem.store32(0x09C122B0, manager);
    auto read = [&] { refresh(ctx, view); return snapshot(); };
    auto actor = [&](unsigned address, unsigned hp, unsigned maximum, unsigned species = 1, unsigned area = 7) {
        ctx.gpr[21] = address; mem.store32(address, 0x08900000);
        mem.store8(address + 0x1E8, species); mem.store16(address + 0x29A, area);
        mem.store16(address + 0x2E4, hp); mem.store16(address + 0x41E, maximum);
        if (address >= 0x09100000 && address < 0x09114000)
            mem.store32(manager + 4640 + ((address - 0x09100000) / 0x1000) * 4, address);
        check(mem.load16(address + 0x2E4) == static_cast<std::uint16_t>(hp) && ctx.gpr[21] == address);
    };
    actor(0x09100000, 2000, 2000); check(read().count == 0);
    set_enabled(true);
    actor(0x09100000, 2000, 2000); check(read().count == 1); // Visible before a hit.
    actor(0x09101000, 3000, 4000, 11); actor(0x09102000, 200, 400, 75);
    auto data = read(); check(data.count == 3); // Not limited to two bars.
    check(std::string(large_monster_name(data.monsters[0].species)) == "Rathian");
    check(std::string(large_monster_name(11)) == "Rathalos");
    actor(0x09103000, 9000, 10000, 3); check(read().count == 3); // Kelbi, even with high HP.
    actor(0x09104000, 5000, 5000, 1, 8); check(read().count == 3); // Off-area boss.
    mem.store16(0x09100000 + 0x29A, 8); check(read().count == 2); // Leaves area without a hit.
    mem.store16(hunter + 0x29A, 8); check(read().count == 2); // Hunter changes area immediately.
    mem.store16(hunter + 0x29A, 98); check(read().count == 0); // Empty area is empty.
    mem.store16(hunter + 0x29A, 7);
    mem.store16(0x09100000 + 0x29A, 7); mem.store16(0x09100000 + 0x2E4, 500);
    data = read(); check(low_hp(data.monsters[0])); // HUD refresh sees damage independently.
    check(!low_hp({1, 1001, 4000, 0}) && !low_hp({1, 0, 4000, 0}));
    mem.store16(0x09100000 + 0x2E4, 0xFFFF); check(read().monsters[0].hp == 0);
    mem.store32(0x09100000, 0); check(read().count == 2); // Despawn invalidates allocation.
    actor(0x09101000, 5000, 4000, 11); check(read().count == 1);
    actor(0x09103000, 1, 0); actor(0x09103000, 1, 0x8000); check(read().count == 1);
    actor(hunter, 150, 30000); check(read().count == 1);
    // Registry membership, rather than damage capture, owns discovery/removal.
    mem.store32(manager + 4640 + 2 * 4, 0); check(read().count == 0);
    actor(0x09105000, 30000, 30000); check(read().count == 1);
    mem.store32(manager + 4640 + 7 * 4, 0x09105000); check(read().count == 1);
    mem.store32(manager + 4640 + 7 * 4, 0);
    mem.store8(state + 46, 3); check(read().count == 1); // Success countdown.
    mem.store8(state + 46, 5); check(read().count == 0); // Reward transition.
    mem.store8(state + 46, 6); check(read().count == 0); // Save screen.
    mem.store8(state + 46, 0); check(read().count == 0); // Village.
    mem.store8(state + 46, 2); check(read().count == 1);
    mem.store32(manager + 4640 + 5 * 4, 0);
    mem.store32(0x09C122B0, 0xFFFFFFFC); check(read().count == 0);
    mem.store32(0x09C122B0, manager);
    mem.store32(0x09AC7AF4, 0x24020009);
    ctx.gpr[21] = 0xFFFFFFFC; capture(ctx, view);
    clear(); check(snapshot().count == 0);
    actor(0x09100000, 1, 2); check(read().count == 1);
    set_enabled(false); check(snapshot().count == 0); check(read().count == 0);
}
