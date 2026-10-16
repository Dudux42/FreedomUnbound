# Flag Key Quests 0.1.0

Author: DuduX42. Game: MHFU USA (ULUS10391), updated native Freedom Unbound launcher.

## Install and use

Drop `build/mods/key-quests-0.1.0.zip` onto the launcher's **Mods** page,
install it and enable **Flag Key Quests**. The desktop shortcut uses the rebuilt
launcher; older executables do not contain this provider.

A small gold five-point star appears immediately to the left of a key quest's
name when browsing Village or Gathering Hall quest pages. Both low and high
Village tiers and low, high and G-rank Hall tiers use the same rule. Completed
key quests remain flagged. The native completion/urgent labels remain visible.
The icon follows the name's actual draw position and disappears with its page.

Classification reads the native progression prerequisite lists: 43 Hall keys
and 34 Village keys across 18 tiers. It does not infer importance from quest
names, monster types, rank or completion status. Ordinary quests outside those
lists receive no star. Enabling/disabling the mod changes only the marker;
it does not unlock quests or modify saves.

## Implementation

This mod was written independently from the local executable's observed data
and rendering behavior. No downloaded mod code, game strings, quest catalogue
or game assets are included in its package or source.

`scripts/instrument-key-quests.py` inserts guarded observation calls in a build
copy of main unit 35, after the existing text-mod instrumentation. It validates
exact labels and original instructions, refusing unsupported or duplicate
instrumentation. The original generated corpus stays untouched.

The common Village/Hall title draw at `0x09A8CE7C` calls the native text wrapper
`0x0889198C`, returning at `0x09A8CE84`. Its s4 register holds the quest record;
the quest ID at +24 is also used by the adjacent native clear-status check.
The mod validates that caller and instruction signatures, reads the ID, and
checks the game's 18 terminated prerequisite lists through the pointer table
at `0x089B2010`. List pointers, bounds, terminators and IDs are validated before
a key is accepted. No guest register or guest memory is changed.

The native title coordinates feed a bounded, synchronized marker queue. The
existing launcher overlay renderer draws a small vector star with a thin dark
border. It consumes queued markers each UI frame and rejects stale entries;
the icon has no dependency on a custom font or a translated name.

Source files:

- `host/key_quests.cpp` and `.hpp`: native ID classification and title capture.
- `host/key_quests_ui.cpp`: provider registration and vector star rendering.
- `scripts/instrument-key-quests.py`: validated build-copy hooks.
- `scripts/package-key-quests.py`: deterministic data-only package.
- `tests/key_quests_tests.cpp`: classification and draw-context guard tests.
- `tests/test_key_quests_instrumentation.py`: injection checks.

Build with the normal Windows CMake configuration. CTest names are
`mhfu_key_quests_tests` and `mhfu_key_quests_instrumentation_tests`. Running
`mhfu_key_quests_tests.exe game/EBOOT.ELF` additionally checks all 77 native
entries directly against the owner's local executable. Game data stays local.

For diagnostic runs only, `MHFU_KEY_QUEST_TRACE=1` logs observed native text
calls; normal desktop launches leave it off.

## Verification

On Windows on 2026-10-15, the rebuilt launcher passed 15 relevant regression
tests. A local executable probe accepted all 77 native keys across all 18 tiers
and rejected non-key controls. Capture tests covered Village and Hall IDs,
wrong callers, invalid pointers, wrong signatures, duplicate markers, disabling
and unchanged guest registers/record data.

An isolated native playtest with interpreter fallback disabled loaded this
package and visually verified stars beside **Hunt the Carnivore!**, **Sinking
Feeling** and **Slay the Blangos!**. **An Anteka in the Snow** stayed unmarked.
Returning to the rank menu removed the quest marker. The run exited on its
scripted quit, and the copied save files retained their original hashes.

Hall and later Village/G-rank classification was checked against the local
native tables; those later menus were not separately visited during this
visual playtest. Their quest cards use the same observed title draw path.
