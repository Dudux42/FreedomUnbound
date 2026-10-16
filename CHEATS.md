# Native progression-testing cheats

Requested by the owner on 11 October 2026. These changes are local development
work; the frozen alpha.1/alpha.2 packages are unchanged.

The launcher has a **Cheats** page. While playing, press **Esc** or **L3+R3**
and select the **Cheats** tab. Each cheat has an independent toggle, and
**Disable all cheats** stops all live bypasses. After using free crafting, restart
the game with that toggle off before smithy transactions resume: the game caches
recipe availability, so transactions are blocked until a reset clears the cache.
Defaults are off. While any cheat is on, the launcher home screen says so.
Choices persist in `cheats.ini` beside the installation's
`settings.ini`, separately from the hunter saves.

- **Unlimited health:** refills to the current maximum, suppresses negative
  health deltas and uses the original no-damage path for incoming attacks.
  Does not revive an already fainted hunter or change maximum health.
- **Unlimited stamina:** refills to the current maximum and blocks action
  costs. It does not increase the maximum stamina or change action speed.
- **One-hit kills:** a monster's decreasing health update becomes zero health.
  Monsters are not killed on spawn. Healing and unchanged HP remain normal.
  The original death, quest and carving code follows the lethal update.
  Environmental or companion damage routed through that update also qualifies.

- **Max money:** keeps 9,999,999 zenny. Normal spending resumes immediately when
  disabled. Existing money is retained.
- **No crafting requirements:** preserves normal progression availability for
  new equipment and decoration recipes, bypasses missing materials for visible recipes, and skips
  material consumption and smithy fees. Enable Max money alongside it to satisfy
  the normal affordability display/checks. Original equipment creation/upgrade
  code and equipment-box capacity checks remain in place. Disable and restart
  to restore normal smithy transactions safely. Upgrades retain their unrestricted
  material bypass. Attach lists only owned decorations and consumes the attached
  jewel normally; the crafting bypass does not create inventory ownership.
- **Max stack on box deposit:** incoming stacks become 99 during normal deposits.
  Manual transfers and shared deposit routines are intercepted; withdrawals,
  removals and empty slots are not filled. Turning it off restores normal deposit
  quantities immediately. Items already obtained remain.
- **Complete quest with L+R:** press the mapped PSP L and R buttons together
  during an active, unpaused quest to invoke the original success path. Default
  PC bindings are **Q + Left Shift**. The game's return countdown, rewards and
  result screens follow normally. A fresh press is required; holding the buttons
  before quest entry does not trigger it. Turning the toggle off immediately
  disables the shortcut; already completed progress remains.

A reset restores the normal rules, not previously earned money, items, equipment
or progression. Preferences remain separate from hunter saves.

These are specific native hooks for USA ULUS10391, not a CWCheat interpreter.
They intercept build-directory copies of twelve generated units, so direct calls
inside an AOT unit cannot bypass them. Original generated files are unchanged.
Runtime image and overlay signatures gate execution; pointers are not cached
across quest/load/area changes. UI flags use atomics; guest writes occur only
on the guest execution thread. Hook events appear as `[fu-cheats]` in logs.

## Address research and provenance

[Abystus's original invincibility and one-hit codes](https://gamehacking.org/game/107154?hacker=Abystus)
provided instruction-address leads, independently checked against the locally
recovered USA gameplay overlay. The invincibility address is `0x09A6B154`;
the original branch's delay slot and exit path are preserved. The monster HP
calculation ends at `0x09AC7AD4`, storing a signed 16-bit value at actor `+0x2E4`.
Our implementation zeroes only decreasing HP, preserving healing.

[The initial USA cheat thread](https://forums.ppsspp.org/showthread.php?tid=5097)
provided health/stamina field leads. The supported image and generated routines
confirm the local hunter at `0x090B3580`: HP `+0x2E4`, max HP `+0x41E`,
stamina `+0x55A`, max stamina `+0x62E`. The health-delta routine starts at
`0x088D6594`, stamina-delta routine at `0x09A6A2D0`; the player update is
observed at `0x088D5E30`. No third-party cheat evaluator or implementation was copied.

## Acceptance and limitations

Synthetic regression checks cover all eight toggle combinations, lethal
health deltas, stamina costs, monster damage versus healing, dead/invalid
hunters, other actors, unsupported images and overlay replacement. Generation
rejects missing, duplicated, changed and already instrumented labels.

Actual quest acceptance must check hits against small and large monsters,
health under attacks/poison/heat, prolonged sprinting/rolling/guarding,
carving/rewards, area transitions, toggles during a hunt and cold save/reload.
Every weapon, damage source and monster is not established as covered. Cheats
change earned progress; combat/difficulty acceptance requires cheats off.
Multiplayer cheat behavior is unverified.

Local verification on 11 October 2026:

- Standard Windows build completed with timing research disabled; all **27/27**
  regression targets passed, including the two new cheat targets.
- Build-copy reversal checks confirm all four original units are unchanged
  after removing only the six hook calls and added include from the copies.
- Native launcher captures verify the Cheats page, independent health/stamina
  selection and persisted preferences. Native village/paused-menu captures
  verify all three switches on the in-game Cheats tab.
- A normal-paced copied-hunter run reached a quest and executed the guarded
  health path. A separate accelerated diagnostic run recorded actual negative
  sprint stamina deltas becoming zero. Accelerated navigation does not validate
  original timing. The health log's `incoming-hit-blocked` events count the
  invulnerability branch checks, not measured enemy contacts or damage.
- The scripted route stayed near base camp; **actual damage immunity, one-hit
  kills against a monster, carving/rewards and cheated progression save/reload
  were subsequently confirmed working by the owner**. This does not establish exhaustive coverage of every damage source or monster. Synthetic damage checks are not a combat
  playtest. All source and diagnostic copied saves stayed unchanged.
- The local package passed dependency/source freshness and all 310 manifest
  entries; the complete framework patch passes reverse-application checking.

Ignored evidence is under `build/windows/investigation/cheats/`; build/test logs
are `build/windows/cheats-build-final.log` and `build/windows/cheats-tests.log`.

`Cheat-Playtest.cmd` uses a persistent copied hunter under
`build/windows/cheats-playtest/game`, initially copied from the owner's alpha
playtest (or the isolated development installation). Initial settings are
30 FPS, 1x resolution and 20% volume, with all three cheats enabled. Repeated
runs preserve this copy's progression and choices. Existing saves and released
packages are not overwritten. The helper selects the newest locally packaged
build declaring cheat hook version 6 or later and verifies its manifest before launch.


## Economy and smithy extension — 11 October 2026

The new hook addresses were derived from the locally recovered USA routines.
Money uses the original bounded 32-bit money arithmetic at `0x0884FBBC`.
Smithy material availability is intercepted at `0x0885E214` and `0x09AA4A4C`;
consumption takes the original empty-material path at `0x09AA5D90`. Only smithy
callers have fees waived. No fake material counts are written to the hunter.
Seven manual item-box quantity stores and three shared deposit stores are
intercepted. The lobby overlay is guarded independently of the combat overlay.

After a material bypass has executed, turning free crafting off sets a restart
requirement. Both smithy transaction routines then take their original epilogues
before fees, inventory or equipment change. Guest initialization clears the
requirement. Turning the cheat back on allows transactions again. This prevents
stale cached availability from leaving free crafting active after it is disabled.

Synthetic checks now cover money/deposit/crafting enable-disable cycles, smithy
versus ordinary shop fees, withdrawal/deposit direction, stale transactions,
reset recovery and lobby replacement. Live acceptance of the new three controls
and every equipment category is separate from these memory fixtures.

Final extension verification:

- Windows build completed; **27/27** regression targets passed, including loaded
  hunter/storage changes, invalid previous pointers and disable/reset fixtures.
- All **11** instrumented copies match the exact supported original instructions
  plus **22** hook calls and the two guarded original smithy epilogue paths.
- Native launcher capture shows all six independent controls. Copied-hunter
  village runs verified the actual loaded storage base and clean bounded exits;
  source and copied saves stayed unchanged. Accelerated navigation runs establish
  menu/startup compatibility, not real-time gameplay or completed transactions.
- New package `FreedomUnite-20261011-045208` passes executable/source freshness,
  DLL dependency and all **310** manifest-entry checks. The complete framework
  patch still passes reverse-application checking.
- `Cheat-Playtest.cmd` now selects hook version 2; preparation selected this new
  package and retained the existing persistent test hunter. New toggles default
  off until selected. Frozen alpha packages are unchanged.
- The owner subsequently confirmed live economy and smithy behavior while
  progressing through village key quests: several armor sets and weapons crafted,
  some weapons upgraded, money at maximum without decreasing, and deposited
  items multiplying correctly. These tested transactions are gameplay-verified;
  exhaustive equipment coverage remains open. The owner also confirmed that
  disabling cheats followed by a reset restores original game behavior.

## Progression-aware forging adjustment — 11 October 2026

The owner reported that enabling the current crafting bypass exposes every
weapon and armor recipe. Requested behavior: keep normal progression-based
availability for newly forged equipment, bypassing material/fee requirements
only for recipes already available through progression. Upgrades may retain
their current behavior.

The owner completed urgent Khezu and authorized the adjustment. The shared
availability return preserves the game's hidden result (0), promoting only
visible recipes (1 or 2) to craftable (2) for both Create-list callers. The
decoration caller retains its existing bypass; unknown callers and invalid
stack addresses are untouched. Weapon unlock checks remain in the
original game code; the shared quantity check and consumption bypass remain.
No recipe tables, unlock flags or material-history flags are changed. Disabling
and resetting retains the previously verified recovery behavior.

Fixtures cover hidden/visible/ready recipes for both Create-list callers,
unchanged quantity/consumption bypasses, unknown callers and invalid stacks.

Build `FreedomUnite-20261011-060304` completed and passes the package manifest,
dependency and freshness audit. That build used hook version 3. All 27 regression
targets passed across validation and individual reruns, but neither full-suite
run was entirely clean: texture-pack cancellation failed in the first run and
damaged-save rejection failed in the second; both passed individual reruns.
The revised native cheat target passed in both full runs. Exact instrumentation
verification still reports 11 copies and 22 hooks with original code retained.

Evidence: `build/windows/cheats-progression-build.log`,
`cheats-progression-tests.log`, `cheats-progression-final-tests.log`,
`cheats-progression-texture-recheck.log`, `cheats-progression-savedata-recheck.log`
and `build/windows/investigation/cheats/progression-package-audit.json`.

The owner tested version 3 and confirmed weapon availability was correct, but
Create Armor still exposed everything. Inspection found that `0x09AA2C60` had
been misclassified as an armor upgrade caller: it builds Create Armor using
recipe kind 0. Menu byte +36 selects weapon/armor, while +37 selects
create/upgrade. Version 4 applies the same hidden-recipe preservation to
`0x09AA2C60` as `0x09AA27D8`. Actual upgrade quantity/consumption hooks are
unchanged. The owner subsequently confirmed version 4's armor list is fixed.

Version 4 package `FreedomUnite-20261011-061402` includes the new branded
executable icon, passes its manifest/dependency/freshness audit and is selected
by `Cheat-Playtest.cmd`. The existing hunter is retained. The complete regression
suite passed **27/27** in one run. Evidence: `build/windows/cheats-armor-visibility-`
build/test/package logs and
`build/windows/investigation/cheats/armor-visibility-package-audit.json`.

## Decoration visibility correction — 11 October 2026

The owner accepted the icon and corrected armor list, then reported that
decorations still exposed the full list. The decoration Create-list caller
`0x09AA7454` had retained the original unrestricted bypass. It now follows the
same availability rule as both equipment Create lists: hidden (0) stays hidden,
visible but missing materials (1) becomes craftable (2), and ready (2) remains
ready. Unexpected results and unknown callers remain untouched. Existing fee,
consumption, upgrade and disable/reset behavior are retained.

Hook version 5 was selected by the persistent helper. Fixtures cover all three
Create callers, locked and unlocked recipes, unexpected results and disabled
behavior. The owner subsequently confirmed the decoration crafting list works
as intended, but reported that Attach still listed every decoration.

Package `FreedomUnite-20261011-062443` passes manifest/dependency/freshness
verification. All **27/27** regression targets passed in one run, and all 11
instrumented copies still match the original instructions plus the 22 hooks.
The helper selects this package and preserves the existing hunter. Evidence:
`build/windows/cheats-decoration-visibility-build.log`,
`build/windows/cheats-decoration-visibility-tests.log` and
`build/windows/investigation/cheats/decoration-visibility-package-audit.json`.

## Decoration Attach ownership correction — 11 October 2026

Hook version 6 preserves the actual pouch/box quantity result for the Attach
list caller `0x09AA77B8`. The shared quantity bypass remains active for crafting.
The attachment consumption caller `0x09AA85A4` also retains its normal inventory
deduction, so attaching the final loose jewel removes it from the available
list. Crafting materials remain unconsumed, and progression visibility, upgrades,
fee bypasses and disable/reset recovery retain their existing behavior.

Fixtures cover owned and unowned Attach results with free crafting on, all
cheats on and cheats off, plus normal attachment deduction and retained crafting
material bypass. All **27/27** regression targets passed in one run; all 11
instrumented copies match their original instructions plus the 22 hooks. Owner
acceptance of the corrected Attach list was subsequently received: the owner
confirmed it works fine.

Package `FreedomUnite-20261011-063200` passes its manifest/dependency/freshness
audit. The persistent playtest helper selects it and retains the existing hunter.

Evidence: `build/windows/cheats-attach-ownership-build.log`,
`build/windows/cheats-attach-ownership-tests.log` and
`build/windows/investigation/cheats/attach-ownership-package-audit.json`.

## Quest completion shortcut and desktop launcher — 11 October 2026

The owner requested a toggle to complete a quest with L+R. Version 7 intercepts
the objective update caller at `0x0886E838`, after its saved registers, and
redirects a valid request to the original success call at `0x0886E8E0`. That call
uses `0x0886E978` to select the current quest script's success entry and enter
the original success state. No save flags or quest IDs are edited by the hook.
Guards check the supported instructions, gameplay overlay, current quest
pointer, loaded live hunter, active state, pause, remaining time, fresh button
chord, caller and bounded script lookup. The toggle defaults off and persists
as `quest_complete` in the separate cheat preferences. Village, expired/failing,
fainted and paused states cannot invoke it.

All **27/27** regression targets passed in one run. Fixtures exercise the fresh
chord, independent toggle, actual update caller, current quest pointer, active
state, pause, timer, fainted hunter, script bounds and instruction signatures.
All **12** build-directory copies match their original instructions plus the
**23** hooks. Both executables retain all seven matching icon sizes.

A bounded isolated native run reached **Quest Cleared — Mountain Herb Picking**,
the original reward menu and its normal reward calculation after one L+R event.
It completed in 53.65 seconds via scripted quit (runtime exit 4); source and copied
saves were unchanged. This check used unthrottled native 30 simulation and audio
disabled to fit the diagnostic bound. Earlier runs reached the active quest and
reward menu but were stopped at the 55-second limit; they do not establish a full
exit loop. Normal paced audio and persistent quest-clear save/reload acceptance,
plus hunting, capture, urgent, arena and guild progression coverage, remain owner
playtests. A shortcut success is not a test of normal objectives.

Final package `FreedomUnite-20261011-065628` passes manifest, dependency and
freshness verification and is selected by the persistent helper. Evidence:
`build/windows/cheats-quest-complete-tests.log`,
`build/windows/investigation/cheats/quest-complete-native-3/` and
`build/windows/investigation/cheats/quest-complete-package-audit-final.json`.

The desktop **Freedom Unbound** shortcut uses the branded ICO and the stable
`scripts/playtest-cheats.ps1` entry. It selects the latest compatible packaged
build and retains `build/windows/cheats-playtest/game`. Future local launcher
updates use this same entry and icon path, so the desktop link follows updates.

Evidence: `build/windows/cheats-economy-storage-build.log`,
`build/windows/cheats-economy-final-tests.log`, and
`build/windows/investigation/cheats/economy-*.json`; screenshots and native logs
are in the corresponding isolated `economy-*` directories.
