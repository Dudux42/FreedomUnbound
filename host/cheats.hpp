#pragma once
#include <cstdint>

namespace psprecomp { class Runtime; class AotFastView; struct AllegrexContext; }

namespace portablekit::fu {
enum Cheat : std::uint32_t { Health = 1u, Stamina = 2u, OneHit = 4u, Money = 8u, BoxStacks = 16u, FreeCraft = 32u, QuestComplete = 64u };
void set_cheats(std::uint32_t mask);
std::uint32_t cheats();
void initialize_cheats(psprecomp::Runtime &);
// Exported by the executable, shared by the compiled gameplay overlay.
void cheat_hook(std::uint32_t point, psprecomp::AllegrexContext &, const psprecomp::AotFastView &,
                std::uint32_t deposit_quantity_address = 0, unsigned value_register = 0);
bool equipment_transaction_allowed(const psprecomp::AotFastView &);
bool crafting_restart_required();
bool quest_complete_requested(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void load_cheat_preferences();
void draw_cheat_rows();
bool show_cheats_page();
}
