# Monster HP: native launcher mod

Modern Camera 0.2.0 can mark its selected monster with a small gold target icon
inside that monster's HP bar. It handles targeting independently; disabling
Monster HP switches Modern Camera to its own target icon and monster-name display.

Author: DuduX42. Version: 0.2.1 (experimental).
Target: the modified Freedom Unbound Windows build, USA ULUS10391, mod API 1.

## Install and behavior

Drop `monster-hp-0.2.1.zip` onto the launcher's Mods page, install it, then
switch Monster HP on. The desktop shortcut already points to the rebuilt
`build/windows/bin/FreedomUniteNative.exe`. The ZIP selects native code in
that build; it cannot add support to an older executable.

The bottom-left HUD shows monster names, current HP, maximum HP and percentage,
with smooth bars stacked upwards. An explicit USA species catalog filters out
small monsters regardless of their HP. Only large monsters whose area matches
the hunter's current area appear; an empty area produces no overlay. Up to
eight records can be displayed in stable actor order. The bar is red above 50%, orange
above 25%, and green at 25% or less, matching the reference's color convention.
Zero HP is labeled DEAD.

LOW HP means only 25% HP or less, not guaranteed capture eligibility. The
reference's CAPTURE badge uses a fixed 25% threshold without checking species,
quest rules, trapping immunity or actual capture thresholds. This native
version deliberately labels the measurement rather than promising capture.

The HUD discovers actors directly from the live 20-slot monster registry on
each gameplay HUD/camera update, so entering a monster's area shows its bar
without requiring a hit. It stays visible over launcher menus and system
dialogs while eligible monsters remain in the area. It takes no input focus.
Position is fixed; the reference's
external-window dragging/right-click behavior is not implemented. Disable
through Mods. There are no configurable settings in this version.

The mod reads health only. It does not change gameplay, saves, cheats or
PPSSPP settings. Native overlay inclusion in F12 screenshots is not guaranteed.

## What was in the supplied archive

The reference archive contained a PowerShell/WPF overlay, two batch launchers,
a PowerShell setup script and a README. It targets FUComplete v1.4's Japanese
ULJM05500 release running in PPSSPP. Its README explicitly excludes vanilla
USA MHFU. The scripts were read as reference documents and were not executed.
In particular, setup instructions to enable PPSSPP's remote debugger were
not treated as a user request to change emulator settings.

SHA-256 of the reference archive:
`0457914D969B15DC73C75BC12D7E95E57C17E03D6F93F2D80D29E4A519198F0A`.

Its implementation polls a pointer chain rooted at `0x08985568` over PPSSPP's
WebSocket debugger, reads eight actor slots, and sorts valid HP records by
maximum HP. HP fields are +0x2E4 current and +0x41E maximum. That release's
root and slot chain are not assumed valid in our USA build. No original
scripts, setup tools, UI code or game files are redistributed in our ZIP.

## Native implementation

Our independently written C++ implementation uses the already identified USA
monster HP processing routine. At `0x09AC7AF4`, `s5` contains the monster actor
after pending damage has been applied and HP has been clamped to its maximum.
The capture reads the final fields directly, so it observes cheat-adjusted
health as well as ordinary damage. It does not guess health by subtracting
damage-number events, and works independently of the damage-number mod.

`scripts/instrument-monster-hp.py` inserts a callback before the original
instruction (`addiu v0, zero, 9`, word `0x24020009`). The hook checks that word
and the HP-store signature at `0x09AC7AD4` (`0xA6A202E4`) at runtime. Build
copies compose with existing cheat instrumentation; generated game sources
remain unchanged.

`host/monster_hp.cpp` validates aligned actor addresses and guest RAM bounds,
excludes the local hunter, rejects invalid maxima and current HP above maximum,
and treats negative signed HP as zero. USA translated routines read these
fields as signed halfwords, so this implementation accepts maxima 1–32767.
Discovery reads the USA monster manager at `u32[0x09C122B0]`, then its 20
actor pointers at `manager + 4640 + slot*4`. This layout is verified from the
spawn-registration store at `0x09AB168C` and its 20-slot loop at `0x09AB16AC`.
The registry is read again on each HUD/camera refresh; removed actors disappear
without requiring another attack. Duplicate pointers are suppressed.
Species byte +0x1E8 supplies the authored name/large-monster catalog; area is
the halfword at +0x29A, also used by the game's area comparisons.

Quest state is `u8[u32[0x089CC558] + 46]`. Only hunting (2) and the success
countdown (3) publish bars. The build-copy hooks at `0x0886F0BC` and
`0x0886F0F8` clear published bars as the quest enters later reward/save states,
even if camera and HUD polling stops. The original phase-store instructions
remain intact. Pause/menu visibility does not rely on a wall-clock timeout.
Only copies cross the mutex into the renderer; drawing never reads guest RAM.

`host/monster_hp_ui.cpp` registers native provider `monster-hp` with capabilities
`overlay_draw` and `monster_hp`. It draws inside the existing ImGui gameplay
frame (including the persistent path over menus), scales with window height, and smoothly interpolates bar width while
the numeric HP stays exact. Actor changes reset interpolation. Discovery is independent of the damage callback. Unusual and multiplayer
monster coverage still needs live verification.

## Build, tests and package

Use the configured Visual Studio developer environment and local supported
game corpus described in `BUILDING.md`:

```powershell
cmake --build build/windows --target FreedomUniteNative overlay_ovl09A5F300_game_task_36BD79C16BF70197 mhfu_monster_hp_tests --parallel 2
ctest --test-dir build/windows -R mhfu_monster_hp --output-on-failure
python scripts/package-monster-hp.py
```

The package contains `mod.json` and this guide at its root. The manifest
identifies DuduX42, USA ULUS10391, API 1 and both required native capabilities,
with `affects_save` false. The reusable profile/menu changes for persistent
overlays are retained in `patches/portablekit-local.patch`.

Tests cover species names, small-monster exclusion, repeated updates, multiple actors, low-HP boundaries,
negative/dead HP, invalid maxima and addresses, player exclusion, wrong
instruction signatures, area changes without hits, empty areas, reset and enable state. Instrumentation
tests verify the original instruction is preserved and mismatches, duplicate
labels and reinstrumentation are rejected.

Previous 0.2.0 verification: the application and gameplay DLL rebuilt, and all
twelve selected CTest targets passed. Labeled 960x544 samples confirmed named
bars stacked upwards at bottom-left. A separate paused-menu check confirmed
that these bars remain visible over the menu and exited normally at frame
100. Evidence: `build/windows/overlay-fixes-live/combined-a.bmp` and
`persistent-menu-fixed.bmp`. These samples do not establish real-hunt spawn,
despawn or multiplayer coverage; the filters and area transitions are tested
against simulated guest memory.

Earlier baseline verification: the Windows native application and instrumented gameplay DLL built
successfully. All twelve selected CTest targets passed, including existing
damage-number, gathering, cheat and instrumentation checks. A labeled sample
preview at 960x544 was visually inspected: both bars, exact sample values,
percentages and the low-HP label fit correctly. That bounded run exited at
frame 70.

A separate normal run used an installed/enabled package and an isolated copy
of game data/saves, with the preview flag absent. It loaded the village and
Snowy Mountains quest camp, loaded the instrumented gameplay DLL, displayed
no sample bars, and exited normally at frame 4550. No monster HP records were
captured in camp. Evidence is local under `build/windows/monster-hp-live/`.
Actual hunt HP tracking, monster death/despawn and multiplayer coverage remain
unverified until tested in combat. Synthetic previews are not gameplay proof.

Developer-only `MHFU_MONSTER_HP_PREVIEW=1` displays explicitly labeled sample
values for layout checks. `MHFU_MONSTER_HP_TRACE=1` logs changed captured HP.
Neither flag is added to the desktop shortcut or required for ordinary use.

## 0.2.1 correction

Replaced damage-triggered discovery with live registry enumeration and added
explicit reward/save transition clearing. Regression fixtures now populate the
registry without invoking the damage callback, and cover area entry/exit,
slot removal, duplicate pointers, invalid roots and quest phases. Instrumentation
tests verify both transition hooks preserve the original store.

The native application and gameplay DLL rebuilt successfully; all 12 selected
CTest targets passed. A bounded normal-HP run reached the village and quest
camp without stale/sample bars and exited at frame 4550. Actual monster-area
entry and the reward transition were verified by guest-memory/instrumentation
regressions, but still require a live hunt playtest.
