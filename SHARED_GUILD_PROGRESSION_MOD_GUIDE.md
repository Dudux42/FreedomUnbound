# Shared Gathering Hall Progression 0.1.0

Author: DuduX42. Supported game: MHFU USA, ULUS10391, in the updated native Freedom Unbound launcher.

## Installation and behavior

Build/package: `build/mods/shared-guild-progression-0.1.0.zip`.
Drop this ZIP onto the launcher's **Mods** page, install it, and enable
**Shared Gathering Hall Progression**. It requires the rebuilt launcher;
the ZIP contains configuration and this guide, not a standalone executable patch.

Each participant who wants shared urgent credit must enable this mod locally.
An enabled host cannot change an unmodified remote player's save.

Successful Gathering Hall quests use the normal local clear handling for all
participants, including urgent quests joined rather than posted. Ordinary Hall
quests already award completion to joined players; this mod removes the extra
poster-only restriction for Hall urgents. Local rank, key-quest prerequisites,
unlock checks and saving still follow the game's normal logic. It does not copy
the host's past progression or directly force an HR value.

Failed, abandoned and unfinished quests receive no additional credit. Village,
training and treasure quests keep their existing behavior. Disabling the mod
restores the original restriction for future completions; it does not erase
earned quest clears already saved. The manifest declares `affects_save: true`
so the launcher's existing save-protection behavior applies.

## Implementation

The reference archive was inspected as data, without running its contents:
`SharedUrgents 14 2.0 2026-06-28T23-10Z mcJYtmjR9.zip`.
SHA-256: `3A4B7EB81DBC99DA74281061F3E1F8E336C9B3D1774785E2E916865C2994A52A`.
Its USA CWCheat file contains urgent-specific masks. This implementation uses
an independent native completion hook instead of copying those masks or shipping
game-derived quest tables.

The native completion routine checks success, online mode and urgent status
before calling the poster-role helper at `0x0887CA08`. At the single completion
branch `0x088751B4`, a joined player's zero result normally skips clear credit.
The hook changes that result to one only for a successful Hall urgent in an
online session. The original clear setter at `0x0884FE10` then records the
current quest through the game's existing clear-bit lookup. The global role
helper is unchanged, including its money/reward callers.

The hook validates the caller, three instruction signatures, RAM pointers,
success state, Hall quest range, online mode, participant role and the game's
own terminated urgent list before allowing credit. It writes no RAM itself.
This is specific to the supported USA executable and fails closed if the
instruction signatures do not match.

Files:

- `host/shared_progression.cpp`: guarded completion gate.
- `host/shared_progression_mod.cpp`: registered QoL provider.
- `scripts/instrument-shared-progression.py`: generated-build callsite injection.
- `host/mod_manager.cpp`: registered native QoL package support.
- `scripts/package-shared-progression.py`: reproducible configuration ZIP.
- `tests/shared_progression_tests.cpp`: guards and optional native-code probe.
- `tests/test_shared_progression_instrumentation.py`: strict injection checks.

Build using the repository's normal Windows CMake configuration. Run
`mhfu_shared_progression_tests` and `mhfu_shared_progression_instrumentation_tests`
with CTest. With the owner's local decrypted ELF, running
`mhfu_shared_progression_tests.exe game/EBOOT.ELF` additionally interprets the
actual native classification, poster check and clear setter in private RAM.
No save or live multiplayer session is touched by that probe.

## Player verification

Windows validation on 2026-10-15: the native launcher and quest overlay built
successfully; all 12 relevant mod tests passed. The private-RAM probe passed
52 native completion cases covering all 11 Hall urgents with poster/joiner and
enabled/disabled controls, ordinary Hall and Village controls, and four player
indices. The packaged QoL provider activated during an isolated native boot
with the interpreter disabled. That run ended on its scripted frame-600 quit
(`window closed`, the launcher's expected exit code 4 for this stop reason).
No user save was loaded or modified by these checks.

Real multiplayer progression and save/reload still require a playtest:

1. Have two players enable the mod and meet the same urgent's local prerequisites.
2. Post the urgent on one player and join with the other; complete it successfully.
3. Confirm completion and any eligible rank/unlock change on both players.
4. Save, restart and confirm the results persist.
5. Repeat with an ordinary Hall quest and, when available, a different quest poster.
6. Fail or abandon a quest and confirm it receives no new clear credit.

The automated native-code probe checks current clear credit for all native Hall
urgents, ordinary Hall and Village controls, and four participant indices. It
does not establish real network synchronization, later rank promotions or save
persistence; those remain the purpose of the playtest above.
