#include "cheats.hpp"
#include "psprecomp/runtime.hpp"
#include <cstdio>
#include <stdexcept>

void require(bool result, const char *message) {
    if (!result) throw std::runtime_error(message);
}
int main() {
    using namespace portablekit::fu;
    psprecomp::Runtime rt;
    auto &mem = rt.memory();
    const auto view = mem.aot_fast_view();
    constexpr std::uint32_t player = 0x090B3580u, monster = 0x09100000u;
    mem.store32(0x088D6594u, 0x27BDFFF0u);
    mem.store32(0x088D5E30u, 0x8E230614u);
    mem.store32(0x08859190u, 0x3C0308A6u);
    mem.store32(0x09A6B154u, 0x106005F7u);
    mem.store32(0x09A6A2D0u, 0x27BDFFF0u);
    mem.store32(0x09AC7AD4u, 0xA6A202E4u);
    mem.store32(0x09A85BACu, 0xA60202E4u);
    initialize_cheats(rt);
    psprecomp::AllegrexContext ctx{};
    for (std::uint32_t mask = 0; mask < 8u; ++mask) {
        mem.store16(player + 0x2E4u, 25);
        mem.store16(player + 0x41Eu, 100);
        mem.store16(player + 0x55Au, 5);
        mem.store16(player + 0x62Eu, 300);
        set_cheats(mask);
        ctx.gpr[17] = player;
        cheat_hook(0x088D5E30u, ctx, view);
        require(mem.load16(player + 0x2E4u) == ((mask & Health) ? 100 : 25), "health independence");
        require(mem.load16(player + 0x55Au) == ((mask & Stamina) ? 300 : 5), "stamina independence");
        ctx.gpr[4] = player;
        ctx.gpr[5] = static_cast<std::uint32_t>(-200);
        cheat_hook(0x088D6594u, ctx, view);
        require(ctx.gpr[5] == ((mask & Health) ? 0u : static_cast<std::uint32_t>(-200)), "lethal health delta");
        ctx.gpr[5] = static_cast<std::uint32_t>(-25);
        cheat_hook(0x09A6A2D0u, ctx, view);
        require(ctx.gpr[5] == ((mask & Stamina) ? 0u : static_cast<std::uint32_t>(-25)), "stamina cost");
        ctx.gpr[3] = 1;
        cheat_hook(0x09A6B154u, ctx, view);
        require(ctx.gpr[3] == ((mask & Health) ? 0u : 1u), "incoming damage branch");
        ctx.gpr[16] = player;
        ctx.gpr[2] = static_cast<std::uint32_t>(-10);
        cheat_hook(0x09A85BACu, ctx, view);
        require(ctx.gpr[2] == ((mask & Health) ? 100u : static_cast<std::uint32_t>(-10)), "overlay lethal health delta");
        ctx.gpr[21] = monster;
        mem.store16(monster + 0x2E4u, 2000);
        for (auto next : {1900u, 2000u, 2100u, static_cast<unsigned>(-10)}) {
            ctx.gpr[2] = next;
            cheat_hook(0x09AC7AD4u, ctx, view);
            require(ctx.gpr[2] == ((mask & OneHit) && static_cast<std::int16_t>(next) < 2000 ? 0u : next), "damage only, not healing");
        }
    }
    constexpr std::uint32_t state = 0x09999DA0u, box = state + 0x3428u;
    mem.store32(0x089CC558u, state);
    // Repeat enabled -> disabled -> enabled in the same guest lifetime.
    for (auto mask : {31u, 0u, 31u, 0u}) {
        set_cheats(mask);
        ctx.gpr[4] = state;
        ctx.gpr[5] = static_cast<std::uint32_t>(-500);
        ctx.gpr[6] = 1000;
        cheat_hook(0x0884FBBCu, ctx, view);
        require(ctx.gpr[6] == ((mask & Money) ? 9999999u : 1000u), "money disable restores original arithmetic");
        require(ctx.gpr[5] == ((mask & Money) ? 0u : static_cast<std::uint32_t>(-500)), "money debit after disable");
        for (auto point : {0x08853B84u, 0x08853BF4u, 0x08853C44u}) {
            ctx.gpr[4] = box;
            ctx.gpr[8] = box + 2;
            ctx.gpr[17] = 7;
            ctx.gpr[16] = 1;
            ctx.gpr[2] = 2;
            ctx.gpr[3] = 99;
            mem.store16(box, 7);
            mem.store16(box + 2, 1);
            const auto reg = point == 0x08853C44u ? 16u : 2u;
            const auto before = ctx.gpr[reg];
            cheat_hook(point, ctx, view);
            require(ctx.gpr[reg] == ((mask & BoxStacks) ? 99u : before), "deposit disable restores normal quantity");
            require(mem.load16(box + 2) == 1, "hook does not mutate box outside original store");
        }
    }
    set_cheats(BoxStacks);
    ctx.gpr[3] = 255;
    ctx.gpr[16] = 1;
    cheat_hook(0x08853C44u, ctx, view);
    require(ctx.gpr[16] == 1, "nonstorable item refused");
    ctx.gpr[3] = 99;
    ctx.gpr[4] = box - 4;
    cheat_hook(0x08853C44u, ctx, view);
    require(ctx.gpr[16] == 1, "outside box refused");
    set_cheats(7);
    // The shared address range contains different overlays in quests/village.
    mem.store32(0x09AA5770u, 0x27BDFDC0u);
    mem.store32(0x09AA49F8u, 0x27BDFFF0u);
    mem.store32(0x09AA5D90u, 0x00A08021u);
    ctx.gpr[29] = 0x09FFF000u;
    mem.store32(ctx.gpr[29] + 12u, 0x09AA92B0u);
    mem.store32(ctx.gpr[29] + 44u, 0x09AA7454u);
    for (auto mask : {63u, 0u, 32u, 0u}) {
        set_cheats(mask);
        for (auto point : {0x0885E214u, 0x09AA4A4Cu, 0x09AA5D90u}) {
            ctx.gpr[2] = point == 0x0885E214u ? 1u : 0u;
            ctx.gpr[5] = 7;
            cheat_hook(point, ctx, view);
            if (point == 0x09AA5D90u)
                require(ctx.gpr[5] == ((mask & FreeCraft) ? 0u : 7u), "materials consumed normally after disable");
            else
                require(ctx.gpr[2] == ((mask & FreeCraft) ? (point == 0x0885E214u ? 2u : 1u) : (point == 0x0885E214u ? 1u : 0u)), "availability bypass stops after disable");
        }
        require(equipment_transaction_allowed(view) == !!(mask & FreeCraft), "stale smithy transaction blocked after disable");
        ctx.gpr[4] = state;
        ctx.gpr[31] = 0x09AA5B5Cu;
        ctx.gpr[5] = static_cast<std::uint32_t>(-500);
        cheat_hook(0x0884FBBCu, ctx, view);
        require(ctx.gpr[5] == ((mask & (Money | FreeCraft)) ? 0u : static_cast<std::uint32_t>(-500)), "smithy fees resume after disable");
        ctx.gpr[31] = 0x08800000u;
        ctx.gpr[5] = static_cast<std::uint32_t>(-500);
        cheat_hook(0x0884FBBCu, ctx, view);
        require(ctx.gpr[5] == ((mask & Money) ? 0u : static_cast<std::uint32_t>(-500)), "free crafting does not waive shop fees");
    }
    require(crafting_restart_required(), "disable reports restart needed");
    for (auto mask : {32u, 0u, 63u}) {
        set_cheats(mask);
        for (auto owned : {0u, 1u}) {
            mem.store32(ctx.gpr[29] + 12u, 0x09AA77B8u);
            ctx.gpr[2] = owned;
            cheat_hook(0x09AA4A4Cu, ctx, view);
            require(ctx.gpr[2] == owned, "Attach lists keep actual ownership with crafting on or off");
            mem.store32(ctx.gpr[29] + 12u, 0x09AA92B0u);
            ctx.gpr[2] = owned;
            cheat_hook(0x09AA4A4Cu, ctx, view);
            require(ctx.gpr[2] == ((mask & FreeCraft) ? 1u : owned), "decoration crafting quantity bypass retained");
        }
        ctx.gpr[31] = 0x09AA85A4u;
        ctx.gpr[5] = 17;
        ctx.gpr[6] = 1;
        cheat_hook(0x09AA5D90u, ctx, view);
        require(ctx.gpr[5] == 17 && ctx.gpr[6] == 1, "Attach consumes its owned jewel normally");
        ctx.gpr[31] = 0x09AA8514u;
        cheat_hook(0x09AA5D90u, ctx, view);
        require(ctx.gpr[5] == ((mask & FreeCraft) ? 0u : 17u), "crafting still preserves recipe materials");
    }
    set_cheats(FreeCraft);
    for (auto caller : {0x09AA27D8u, 0x09AA2C60u, 0x09AA7454u, 0x08800000u}) {
        mem.store32(ctx.gpr[29] + 44u, caller);
        for (auto availability : {0u, 1u, 2u, 3u}) {
            ctx.gpr[2] = availability;
            cheat_hook(0x0885E214u, ctx, view);
            const auto expected = caller != 0x08800000u && (availability == 1u || availability == 2u) ? 2u : availability;
            require(ctx.gpr[2] == expected, "equipment and decoration lists preserve locks; invalid results and unknown callers unchanged");
        }
    }
    ctx.gpr[29] = 0;
    ctx.gpr[2] = 1;
    cheat_hook(0x0885E214u, ctx, view);
    require(ctx.gpr[2] == 1, "invalid smithy stack refused");
    ctx.gpr[29] = 0x09FFF000u;
    mem.store32(ctx.gpr[29] + 44u, 0x09AA27D8u);
    initialize_cheats(rt);
    require(!crafting_restart_required() && equipment_transaction_allowed(view), "reset restores ordinary smithy");
    set_cheats(BoxStacks);
    mem.store16(box, 7);
    mem.store8(0x0899D2BBu + 7u * 24u, 99);
    mem.store16(box + 2u, 10);
    for (auto quantity : {0u, 9u, 10u, 11u, 100u}) {
        ctx.gpr[2] = quantity;
        cheat_hook(0x09AE0000u, ctx, view, box + 2u, 2u);
        require(ctx.gpr[2] == (quantity == 11 ? 99u : quantity), "manual deposit increases only, withdrawals unchanged");
    }
    set_cheats(0);
    ctx.gpr[2] = 11;
    cheat_hook(0x09AE0000u, ctx, view, box + 2u, 2u);
    require(ctx.gpr[2] == 11, "manual box deposit disabled immediately");
    constexpr std::uint32_t next_state = 0x09800000u, next_box = next_state + 0x3428u;
    mem.store32(0x089CC558u, next_state);
    mem.store32(next_state + 0x696F0u, 100);
    set_cheats(Money | BoxStacks);
    ctx.gpr[4] = state;
    ctx.gpr[5] = static_cast<std::uint32_t>(-500);
    ctx.gpr[6] = 1000;
    cheat_hook(0x0884FBBCu, ctx, view);
    require(ctx.gpr[6] == 1000 && ctx.gpr[5] == static_cast<std::uint32_t>(-500), "previous hunter money pointer refused");
    ctx.gpr[4] = next_state;
    cheat_hook(0x0884FBBCu, ctx, view);
    require(ctx.gpr[6] == 9999999 && ctx.gpr[5] == 0, "new hunter money base resolved");
    ctx.gpr[2] = 11;
    cheat_hook(0x09AE0000u, ctx, view, box + 2u, 2u);
    require(ctx.gpr[2] == 11, "previous hunter box pointer refused");
    mem.store16(next_box, 7);
    mem.store16(next_box + 2u, 10);
    cheat_hook(0x09AE0000u, ctx, view, next_box + 2u, 2u);
    require(ctx.gpr[2] == 99, "new hunter box base resolved");
    mem.store32(0x089CC558u, 0);
    ctx.gpr[2] = 11;
    cheat_hook(0x09AE0000u, ctx, view, next_box + 2u, 2u);
    require(ctx.gpr[2] == 11, "unloaded hunter storage refused");
    mem.store32(0x089CC558u, state);
    mem.store32(0x09AA5770u, 0);
    set_cheats(FreeCraft | BoxStacks);
    ctx.gpr[2] = 0;
    cheat_hook(0x0885E214u, ctx, view);
    require(ctx.gpr[2] == 0, "lobby overlay replacement refuses crafting hook");
    set_cheats(7);
    ctx.gpr[4] = monster;
    ctx.gpr[5] = static_cast<std::uint32_t>(-5);
    cheat_hook(0x088D6594u, ctx, view);
    require(ctx.gpr[5] == static_cast<std::uint32_t>(-5), "other actors unaffected");
    mem.store16(player + 0x2E4u, 0);
    ctx.gpr[17] = player;
    cheat_hook(0x088D5E30u, ctx, view);
    require(mem.load16(player + 0x2E4u) == 0, "no resurrection");
    mem.store16(player + 0x2E4u, 10);
    mem.store16(player + 0x41Eu, 65535);
    cheat_hook(0x088D5E30u, ctx, view);
    require(mem.load16(player + 0x2E4u) == 10, "invalid player state refused");
    ctx.gpr[21] = monster;
    ctx.gpr[2] = 1900;
    mem.store32(0x09A6B154u, 0); // A different overlay now occupies this slot.
    cheat_hook(0x09AC7AD4u, ctx, view);
    require(ctx.gpr[2] == 1900, "overlay replacement refuses hook");
    // The shortcut requests the original success path; it never writes quest
    // state or save/progression flags itself, including when rejected.
    constexpr std::uint32_t quest = 0x09A05F00u, script = 0x09810000u;
    mem.store32(0x09A6B154u, 0x106005F7u);
    mem.store32(0x0886E838u, 0x00804021u);
    mem.store32(0x0886E978u, 0x3C03089Du);
    mem.store32(0x0886E9E8u, 0xA064002Eu);
    mem.store32(0x08A62D9Cu, quest);
    mem.store32(quest + 84u, script);
    mem.store16(script + 24u, 0xFFFFu);
    mem.store32(quest + 16u, 90000u);
    mem.store8(state + 46u, 2);
    mem.store8(state + 0x422u, 0);
    mem.store16(player + 0x41Eu, 100);
    mem.store16(player + 0x2E4u, 100);
    ctx.gpr[4] = quest;
    ctx.gpr[5] = 1;
    ctx.gpr[31] = 0x0886B2C8u;
    for (auto mask : {0u, 63u, 64u, 127u}) {
        set_cheats(mask);
        for (auto held : {0u, 0x100u, 0x200u, 0x300u}) {
            mem.store32(0x08A62D88u, held);
            for (auto pressed : {0u, 0x100u, 0x200u, 0x300u}) {
                mem.store32(0x08A62D8Cu, pressed);
                const bool expected = (mask & QuestComplete) && held == 0x300u && pressed != 0;
                require(quest_complete_requested(ctx, view) == expected, "quest shortcut needs its toggle and a fresh shoulder chord");
                require(mem.load8(state + 46u) == 2 && mem.load32(quest + 16u) == 90000u,
                        "request preserves quest state for original success routine");
            }
        }
    }
    set_cheats(QuestComplete);
    mem.store32(0x08A62D88u, 0x300u);
    mem.store32(0x08A62D8Cu, 0x300u);
    for (auto phase : {0u, 1u, 3u, 4u, 5u, 6u, 7u}) {
        mem.store8(state + 46u, phase);
        require(!quest_complete_requested(ctx, view), "quest shortcut refused outside active quest");
    }
    mem.store8(state + 46u, 2);
    mem.store8(state + 0x422u, 1);
    require(!quest_complete_requested(ctx, view), "paused quest refused");
    mem.store8(state + 0x422u, 0);
    mem.store32(quest + 16u, 0);
    require(!quest_complete_requested(ctx, view), "expired quest refused");
    mem.store32(quest + 16u, 90000);
    mem.store16(player + 0x2E4u, 0);
    require(!quest_complete_requested(ctx, view), "fainted hunter refused");
    mem.store16(player + 0x2E4u, 100);
    ctx.gpr[5] = 0;
    require(!quest_complete_requested(ctx, view), "non-mutating objective query refused");
    ctx.gpr[5] = 1;
    ctx.gpr[31] = 0;
    require(!quest_complete_requested(ctx, view), "unknown objective caller refused");
    ctx.gpr[31] = 0x0886B2C8u;
    mem.store32(0x08A62D9Cu, quest + 4u);
    require(!quest_complete_requested(ctx, view), "stale quest pointer refused");
    mem.store32(0x08A62D9Cu, quest);
    mem.store32(quest + 84u, 0);
    require(!quest_complete_requested(ctx, view), "invalid quest script refused");
    mem.store32(quest + 84u, script);
    mem.store16(script + 24u, 0);
    require(!quest_complete_requested(ctx, view), "missing success script entry refused");
    mem.store16(script + 24u, 0xFFFFu);
    mem.store32(0x09A6B154u, 0);
    require(!quest_complete_requested(ctx, view), "non-gameplay overlay refused");
    mem.store32(0x09A6B154u, 0x106005F7u);
    mem.store32(0x0886E9E8u, 0);
    require(!quest_complete_requested(ctx, view), "changed completion signature refused");
    mem.store32(0x088D5E30u, 0);
    initialize_cheats(rt);
    ctx.gpr[4] = player;
    ctx.gpr[5] = static_cast<std::uint32_t>(-200);
    mem.store16(player + 0x41Eu, 100);
    cheat_hook(0x088D6594u, ctx, view);
    require(ctx.gpr[5] == static_cast<std::uint32_t>(-200), "unsupported image refused");
    std::puts("Cheat isolation, damage, lifecycle and signature checks passed.");
}
