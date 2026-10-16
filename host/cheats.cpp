#include "cheats.hpp"
#include "psprecomp/runtime.hpp"
#include <atomic>
#include <cstdio>

namespace portablekit::fu {
namespace {
std::atomic<std::uint32_t> enabled{};
std::atomic<bool> craft_used{}, craft_restart{};
bool supported{}; // Only the guest execution thread uses this.
bool money_state_reported{};
constexpr std::uint32_t player = 0x090B3580u;
constexpr std::uint32_t hp = 0x2E4u, max_hp = 0x41Eu;
constexpr std::uint32_t stamina = 0x55Au, max_stamina = 0x62Eu;
std::uint64_t interventions[7]{};
constexpr std::uint32_t money_offset = 0x696F0u, box_offset = 0x3428u;
std::uint32_t hunter_state(const psprecomp::AotFastView &mem) {
    const auto state = mem.aot_load32(0x089CC558u);
    // The game supplies this base to its money/inventory routines. Resolve it
    // afresh after loads; never retain a previous hunter's storage pointer.
    if (state < 0x08800000u || state > 0x09F90000u || (state & 3u) ||
        mem.aot_load32(state + money_offset) > 9999999u) return 0;
    return state;
}
bool loaded_hunter(const psprecomp::AotFastView &mem) {
    return hunter_state(mem) != 0;
}
bool box_slot(std::uint32_t address, const psprecomp::AotFastView &mem) {
    const auto state = hunter_state(mem);
    if (!state) return false;
    const auto box = state + box_offset;
    return address >= box && address < box + 4000u && ((address - box) & 3u) == 0u;
}

bool valid_player(const psprecomp::AotFastView &mem, std::uint32_t object) {
    if (object != player) return false;
    const auto maximum = mem.aot_load16(player + max_hp);
    const auto current = mem.aot_load16(player + hp);
    return maximum > 0u && maximum <= 150u && current > 0u && current <= maximum;
}
bool gameplay_overlay(const psprecomp::AotFastView &mem) {
    return mem.aot_load32(0x09A6B154u) == 0x106005F7u &&
           mem.aot_load32(0x09AC7AD4u) == 0xA6A202E4u &&
           mem.aot_load32(0x09A85BACu) == 0xA60202E4u;
}
bool lobby_overlay(const psprecomp::AotFastView &mem) {
    return mem.aot_load32(0x09AA5770u) == 0x27BDFDC0u &&
           mem.aot_load32(0x09AA49F8u) == 0x27BDFFF0u &&
           mem.aot_load32(0x09AA5D90u) == 0x00A08021u;
}
void report(unsigned index, const char *name, std::uint32_t object, int before, int after) {
    const auto count = ++interventions[index];
    // First event and powers of two keep diagnostic output bounded.
    if ((count & (count - 1u)) == 0u)
        std::printf("[fu-cheats] %s object=0x%08x before=%d after=%d events=%llu\n",
                    name, object, before, after, static_cast<unsigned long long>(count));
}
void refill(const psprecomp::AotFastView &mem, std::uint32_t mask) {
    if (!valid_player(mem, player)) return;
    if (mask & Health) {
        const auto maximum = mem.aot_load16(player + max_hp);
        const auto current = mem.aot_load16(player + hp);
        if (current < maximum) {
            mem.aot_store16(player + hp, maximum);
            mem.aot_store16(player + 0x41Cu, maximum); // Recoverable red health.
            report(0, "health-refill", player, current, maximum);
        }
    }
    if (mask & Stamina) {
        const auto maximum = mem.aot_load16(player + max_stamina);
        const auto current = mem.aot_load16(player + stamina);
        if (maximum > 0u && maximum <= 450u && current < maximum) {
            mem.aot_store16(player + stamina, maximum);
            report(1, "stamina-refill", player, current, maximum);
        }
    }
}
}
void set_cheats(std::uint32_t mask) {
    const auto previous = enabled.exchange(mask & 127u, std::memory_order_relaxed);
    if ((previous & FreeCraft) && !(mask & FreeCraft) && craft_used.load()) craft_restart.store(true);
    std::printf("[fu-cheats] health=%u stamina=%u one-hit=%u money=%u box=%u crafting=%u quest-complete=%u\n",
                !!(mask & Health), !!(mask & Stamina), !!(mask & OneHit),
                !!(mask & Money), !!(mask & BoxStacks), !!(mask & FreeCraft), !!(mask & QuestComplete));
}
std::uint32_t cheats() { return enabled.load(std::memory_order_relaxed); }
bool crafting_restart_required() { return craft_restart.load() && !(cheats() & FreeCraft); }
bool quest_complete_requested(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    // Called after the objective-check routine saves its registers. Redirect
    // only its real update caller to the game's existing success call/epilogue.
    if (!(cheats() & QuestComplete) || !supported || !gameplay_overlay(mem) ||
        ctx.gpr[5] != 1u || ctx.gpr[31] != 0x0886B2C8u ||
        mem.aot_load32(0x0886E838u) != 0x00804021u ||
        mem.aot_load32(0x0886E978u) != 0x3C03089Du ||
        mem.aot_load32(0x0886E9E8u) != 0xA064002Eu) return false;
    const auto state = hunter_state(mem), quest = ctx.gpr[4];
    if (!state || quest < 0x08800000u || quest > 0x09FFF000u || (quest & 3u) ||
        quest != mem.aot_load32(0x08A62D9Cu) || !valid_player(mem, player) ||
        mem.aot_load8(state + 46u) != 2u || mem.aot_load8(state + 0x422u) != 0u ||
        static_cast<std::int32_t>(mem.aot_load32(quest + 16u)) <= 0) return false;
    constexpr std::uint32_t shoulders = 0x0300u;
    if ((mem.aot_load32(0x08A62D88u) & shoulders) != shoulders ||
        !(mem.aot_load32(0x08A62D8Cu) & shoulders)) return false;
    // The original success routine searches the current quest's script for
    // its -1 success entry. Refuse an invalid/missing entry before redirecting.
    const auto script = mem.aot_load32(quest + 84u);
    if (script < 0x08800000u || script > 0x09FFF000u || (script & 3u)) return false;
    for (unsigned row = 0; row < 512u; ++row) {
        if (mem.aot_load16(script + row * 8u) != 0xFFFFu) continue;
        report(6, "quest-complete-L+R", quest, 0, 1);
        return true;
    }
    return false;
}
bool equipment_transaction_allowed(const psprecomp::AotFastView &mem) {
    if (!supported || !loaded_hunter(mem) || !lobby_overlay(mem)) return true;
    // Availability is cached by the smithy. Never execute a stale transaction
    // after disabling the bypass; a guest reset rebuilds those menus normally.
    if (crafting_restart_required()) {
        report(5, "craft-blocked-until-restart", 0, 1, 0);
        return false;
    }
    return true;
}
void initialize_cheats(psprecomp::Runtime &runtime) {
    const auto &mem = runtime.memory();
    supported = mem.contains(0x088D6594u, 4u) &&
        mem.load32(0x088D6594u) == 0x27BDFFF0u &&
        mem.load32(0x088D5E30u) == 0x8E230614u &&
        mem.load32(0x08859190u) == 0x3C0308A6u;
    for (auto &count : interventions) count = 0;
    craft_used.store(false);
    craft_restart.store(false);
    money_state_reported = false;
    std::printf("[fu-cheats] supported-USA-hooks=%u; cheats modify live quest behavior\n", supported);
}
void cheat_hook(std::uint32_t point, psprecomp::AllegrexContext &ctx,
                const psprecomp::AotFastView &mem, std::uint32_t deposit_quantity_address,
                unsigned value_register) {
    const auto mask = cheats();
    if (!mask || !supported) return;
    const bool lobby = loaded_hunter(mem) && lobby_overlay(mem);
    if (point >= 0x09A5F300u && !gameplay_overlay(mem) && !lobby) return;
    if ((mask & Money) && lobby) mem.aot_store32(hunter_state(mem) + money_offset, 9999999u);
    if (deposit_quantity_address && value_register > 0 && value_register < 32) {
        if (!(mask & BoxStacks) || !lobby) return;
        const auto slot = deposit_quantity_address - 2u;
        if (!box_slot(slot, mem)) return;
        const auto id = mem.aot_load16(slot);
        const auto before = static_cast<std::int16_t>(mem.aot_load16(slot + 2u));
        const auto after = static_cast<std::int16_t>(ctx.gpr[value_register]);
        if (id > 0 && id < 0x800u && mem.aot_load8(0x0899D2BBu + id * 24u) != 255u &&
            after > before && after > 0 && after <= 99) {
            ctx.gpr[value_register] = 99;
            report(4, "box-deposit", slot, after, 99);
        }
        return;
    }
    switch (point) {
    case 0x0885E214u: {
        if (!(mask & FreeCraft) || !lobby) break;
        const auto stack = ctx.gpr[29];
        if (stack < 0x08800000u || stack > 0x09FFFFD0u || (stack & 3u)) break;
        const auto caller = mem.aot_load32(stack + 44u);
        // Forge recipe lists: 0 is hidden by progression/key-material history;
        // 1 is already visible but lacks materials; 2 is ready to craft.
        // Preserve the original visibility decision, including quest flags.
        // Menu +36 selects weapon/armor, +37 selects create/upgrade.
        // Equipment and decoration Create lists share this availability result.
        if (caller != 0x09AA27D8u && caller != 0x09AA2C60u && caller != 0x09AA7454u) break;
        if (ctx.gpr[2] != 1u && ctx.gpr[2] != 2u) break;
        const auto before = ctx.gpr[2];
        ctx.gpr[2] = 2u;
        craft_used.store(true);
        report(5, "free-crafting-visible-recipe", point, before, 2);
        break;
    }
    case 0x09AA4A4Cu:
        if ((mask & FreeCraft) && lobby) {
            const auto stack = ctx.gpr[29];
            if (stack < 0x08800000u || stack > 0x09FFFFF0u || (stack & 3u)) break;
            // Attach lists must use the real pouch/box count, not craftability.
            if (mem.aot_load32(stack + 12u) == 0x09AA77B8u) break;
            ctx.gpr[2] = 1u;
            craft_used.store(true);
            report(5, "free-crafting-quantity", point, 0, 1);
        }
        break;
    case 0x09AA5D90u:
        if ((mask & FreeCraft) && lobby) {
            // Attaching transfers an owned jewel into equipment. Preserve that
            // deduction so the last loose copy disappears from the Attach list.
            if (ctx.gpr[31] == 0x09AA85A4u) break;
            craft_used.store(true);
            ctx.gpr[5] = 0; // Original empty-material path for crafting.
            report(5, "free-crafting", point, 0, 1);
        }
        break;
    case 0x088D5E30u:
        if ((mask & Money) && ctx.gpr[17] == player && !money_state_reported) {
            std::printf("[fu-cheats] hunter-state=0x%08x loaded=%u\n",
                        mem.aot_load32(0x089CC558u), loaded_hunter(mem));
            money_state_reported = true;
        }
        if ((mask & Money) && ctx.gpr[17] == player && loaded_hunter(mem))
            mem.aot_store32(hunter_state(mem) + money_offset, 9999999u);
        if (ctx.gpr[17] == player) refill(mem, mask);
        break;
    case 0x0884FBBCu:
        if ((mask & Money) && loaded_hunter(mem) && ctx.gpr[4] == hunter_state(mem)) {
            ctx.gpr[6] = 9999999u;
            ctx.gpr[5] = 0;
            report(3, "max-money", ctx.gpr[4], 0, 9999999);
        } else if ((mask & FreeCraft) && lobby && ctx.gpr[4] == hunter_state(mem) &&
                   (ctx.gpr[31] == 0x09AA5B5Cu || ctx.gpr[31] == 0x09AA83C4u)) {
            ctx.gpr[5] = 0;
            craft_used.store(true);
        }
        break;
    case 0x08853B84u:
    case 0x08853BF4u:
    case 0x08853C44u: {
        if (!(mask & BoxStacks) || !loaded_hunter(mem) || ctx.gpr[3] != 99u) break;
        const auto slot = point == 0x08853B84u ? ctx.gpr[8] - 2u : ctx.gpr[4];
        const auto value_reg = point == 0x08853C44u ? 16u : 2u;
        const auto quantity = static_cast<std::int16_t>(ctx.gpr[value_reg]);
        if (box_slot(slot, mem) && mem.aot_load16(slot) == (ctx.gpr[17] & 65535u) &&
            ctx.gpr[17] != 0u && quantity > 0 && quantity <= 99 &&
            static_cast<std::int16_t>(ctx.gpr[16]) > 0) {
            ctx.gpr[value_reg] = 99;
            report(4, "box-stack", slot, quantity, 99);
        }
        break;
    }
    case 0x088D6594u: // Health delta, before its argument is saved.
        if ((mask & Health) && valid_player(mem, ctx.gpr[4])) {
            refill(mem, mask & Health);
            if (static_cast<std::int16_t>(ctx.gpr[5]) < 0) {
                report(0, "health-delta-blocked", player, static_cast<std::int16_t>(ctx.gpr[5]), 0);
                ctx.gpr[5] = 0;
            }
        }
        break;
    case 0x09A6B154u: // Original BEQ, including its original delay slot.
        if ((mask & Health) && mem.aot_load32(point) == 0x106005F7u && valid_player(mem, ctx.gpr[4])) {
            ctx.gpr[3] = 0; // Take the game's existing no-damage path.
            report(0, "incoming-hit-blocked", player, 1, 0);
        }
        break;
    case 0x09A6A2D0u: // Local hunter's stamina delta.
        if ((mask & Stamina) && mem.aot_load32(point) == 0x27BDFFF0u && valid_player(mem, ctx.gpr[4])) {
            refill(mem, mask & Stamina);
            if (static_cast<std::int16_t>(ctx.gpr[5]) < 0) {
                report(1, "stamina-cost-blocked", player, static_cast<std::int16_t>(ctx.gpr[5]), 0);
                ctx.gpr[5] = 0;
            }
        }
        break;
    case 0x09A85BACu: // Overlay health delta, before store and lethal checks.
        if ((mask & Health) && mem.aot_load32(point) == 0xA60202E4u && valid_player(mem, ctx.gpr[16])) {
            const auto maximum = mem.aot_load16(player + max_hp);
            if (static_cast<std::int16_t>(ctx.gpr[2]) < maximum) ctx.gpr[2] = maximum;
        }
        break;
    case 0x09AC7AD4u: { // Monster HP store after damage/recovery arithmetic.
        const auto object = ctx.gpr[21];
        // No actor pointers are retained across loads or overlay replacement.
        if (!(mask & OneHit) || mem.aot_load32(point) != 0xA6A202E4u || object == player ||
            object < 0x08800000u || object > 0x09FFF000u || (object & 3u)) return;
        const auto previous = static_cast<std::int16_t>(mem.aot_load16(object + hp));
        const auto next = static_cast<std::int16_t>(ctx.gpr[2]);
        if (previous > 0 && next < previous) {
            ctx.gpr[2] = 0;
            report(2, "one-hit-kill", object, previous, 0);
        }
        break;
    }
    default: break;
    }
}
}
