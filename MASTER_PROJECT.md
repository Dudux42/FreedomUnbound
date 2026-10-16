# Freedom Unbound — Master Project Record

Updated: **15 October 2026**, America/Sao_Paulo.

This is the consolidated project record and handoff document for a fresh agent.
It summarizes work recorded in this conversation and the repository notes;
the linked investigations retain the detailed measurements. It does not claim
that undocumented gameplay paths were tested. Older documents contain dated
statements such as “not published” or “test pending”; use the current-state
sections here for the snapshot at this date and their dated sections as history.

## 1. Current state

**Freedom Unbound is a playable Windows alpha, available privately to invited
testers. It is not yet beta.** The base gameplay runs at the original 30 FPS.
The owner has completed multiple hunting/save/reload loops. Full original
single-player progression, every weapon and broad hardware coverage remain
unverified. No outstanding progression blocker has been established in the
tested loop; this does not establish that the entire game has no blockers.

| Area | Current status |
| --- | --- |
| Base hunting loop | Owner verified village, hunt, combat, completion, return, save and cold reload. |
| Items | Potion, first-aid medicine, hot drink, ration, whetstone and pickaxe mining verified. Other items need coverage. |
| Saves | Writes and reloads work in tested sessions; read-only launcher previews and copied-save backup/export checked. |
| Native code coverage | All 298 recovered game overlay libraries compile and package. Execution of every library is not verified. |
| Jungle performance | Sparse indexed geometry fix resolved the owner's reported sustained slowdown; broader performance testing remains. |
| Launcher | Branded green interface, logo and three save cards; Play above the cards. Compact textured navigation fits without scrolling, with status/language controls at the bottom. Cards include Pokke/Guild Points and equipped gear below the rotatable model. Local Windows builds embed a logo-derived mountain/gem icon in both executables, with seven sizes from 16 to 256 pixels. |
| Distribution | Private alpha.1 and alpha.2 prereleases published. Alpha.2 adds `FreedomUnbound.exe`. |
| Automated checks | All 25 regression targets pass on the final alpha.2 build. Package, dependencies and executable startup checks pass. |
| 60 FPS interpolation | Available but experimental; hunter's house background distortion remains known. |
| Native 60 FPS simulation | Research incomplete and deferred; hooks compiled out of the alpha. |
| Mods | Requested bundles studied; implementation paused by the owner. Texture management foundation exists. |
| Desert | Owner reported the Desert map works perfectly while progressing through village key quests on 11 October. Individual area/monster coverage was not specified. |
| Monster cutscenes | Owner reports all monster cutscenes encountered play correctly with synchronized audio on 11 October. Specific monsters were not enumerated. |
| Cheats | Owner verified all six earlier controls, disable/reset recovery, progression-aware crafting lists and owned-only decoration Attach. Version 7 adds optional L+R quest completion: 27/27 regression checks pass and a copied-hunter native run reached Quest Cleared and normal rewards for Mountain Herb Picking. Persistent progression and other quest types await owner acceptance. Actual upgrade bypasses are retained. Released alpha.2 is unchanged. See CHEATS.md. |
| Standalone save editor — 14 October | Separate FreedomUnboundSaveEditor.exe edits Zenny, Pokke Points and item-box contents. Searchable installed-game catalogue; earliest empty unlocked slots; exact automatic backups. Fixed rejection of existing stacks above 99 after reproducing the owner's stack of 255. Currency/item edits and GUI opening verified on a copy of that save; originals unchanged. All 37 regression targets pass. Earlier native check loaded an edited copied save. 15 October adds a dedicated Save button for queued items and currency, verified after reopening a copy of the two-character save. The editor now defaults to the current FreedomUnite profile and offers an explicit profile selector after both profiles acquired two hunters. Original point-award/spend code verified against an edited copy. See SAVE_EDITOR.md. |
| Launcher portraits — 15 October | Taller save cards display cached Character Select portraits with each hunter's equipped armor and weapon, above existing details. Both current hunters verified in the installed launcher; 960x544 and 480x272 layouts inspected. Cache seeded from copied saves; originals unchanged. Portraits refresh after highlighting a hunter in Character Select. Five relevant test targets and the real-format preview fixture pass. Automatic loading is now available through the separate Load control below. See SAVE_PREVIEW.md. |
| Model previews — 15 October | All occupied launcher slots support rotatable fixed-pose meshes with equipped armor/weapon and transparent backgrounds. Hand cursor and drag ownership; maximize texture-budget fix verified. Materials now follow the enabled texture pack asynchronously with original fallback and PNG/INI edit refresh. FUM2 retains original texture identity; legacy caches remain readable. Two model/material regression targets pass. Native checks with the installed pack matched 1/8 Luis and 6/8 Invoker materials and verified HD rotation/maximize/restore. Empty slots stay empty. Animation and direct equipment-asset loading remain later work. See SAVE_PREVIEW.md. |
| Launcher Load — 15 October | Load buttons above occupied model panels start the selected hunter through normal game loading, with a silent branded loading screen using the launcher scenery and logo, plus Esc cancellation. Temporary audio suppression restores the current volume/mute on arrival or cancellation without changing preferences. USA title/selector guards, decoded-save and live-hunter identity checks, neutral input handoff and timeout recovery. Native copied-save tests reached the village as Luis and Invoker, including Invoker at normal speed; pause/resume after handoff, Esc cancellation, changed-summary rejection and ordinary Play checked. All four relevant regression targets pass. Installed launcher and desktop shortcut checked; original save hashes unchanged. Third-slot navigation has unit coverage; no occupied third hunter was available for a native load test. See SAVE_PREVIEW.md. |
| Multiplayer | Unverified; not established as a working supported release feature. |
| Second PC | Owner plans a weaker-laptop test; no result recorded yet. |

## 2. Owner decisions and working rules

- Preserve the working prototype and previously validated packages. Build and
  test in separate locations; do not overwrite original saves.
- Optimize the base game and launcher first. **Do not resume mod implementation
  until the owner explicitly requests it.**
- Native 60 FPS is the eventual preference, but research is deferred during
  alpha stability work. Interpolation is a separate existing feature.
- Cheats follow the first alpha; health, stamina, one-hit kills, max money, no smithy material requirements and max stacks on box deposit are authorized for progression testing. The owner accepts a game restart when needed to restore normal rules after disabling.
- Crafting adjustment requested on 11 October: preserve progression-based recipe availability for newly forged weapons, armor and decorations while retaining the material/fee bypass for available recipes. Keep upgrades as currently implemented. The owner completed urgent Khezu and authorized implementation, then requested decorations after accepting the equipment lists.
- Decoration Attach must list only decorations the player owns, retaining normal jewel transfer into equipment. The owner accepted the corrected decoration crafting list and requested this Attach correction.
- The owner accepted the corrected Attach list, then requested an optional quest-complete shortcut on L+R for progression testing.
- Keep the desktop Freedom Unbound shortcut current whenever updating the local launcher. The branded shortcut uses the stable playtest script and icon paths, so package updates follow automatically while retaining the current hunter.
- Modern camera scope includes smooth right-stick motion **and press-L targeting
  toward a valid large monster**, while preserving held-L item controls.
- Use **20% audio volume for automated/manual diagnostic runs arranged by the
  agent**. This is a testing preference, not a forced shipping volume setting.
- Keep the GitHub repository **private for invited testers**. Do not change
  visibility or invite/message people without an appropriate owner request.
- Record what a test proves and what it does not. Compilation, startup or an
  intact manifest does not prove complete gameplay compatibility.
- Keep ISO images, original/extracted executable images, generated game code,
  extracted overlays, analysis dumps and saves out of Git. Release packages
  contain the compiled runtime/libraries, not the user's game image or saves.

See [GROUND_RULES.md](GROUND_RULES.md) and [PROTOTYPE.md](PROTOTYPE.md).

## 3. Project structure and build model

Workspace: the project folder.
Repository: [Dudux42/FreedomUnbound](https://github.com/Dudux42/FreedomUnbound).
GitHub reported the rename from `FUrecomp` while this master record was pushed;
the old address redirects. The local origin still uses the old address and can
be updated during repository housekeeping.

The PSP program is translated ahead of time into native C++. PortableKit supplies
the host runtime: Vulkan rendering, audio, controls, guest memory, installation,
saves and system dialogs. This is a Freedom Unite profile on PortableKit, not
a copied Yakumo game profile.

| Location | Purpose |
| --- | --- |
| `host/` | FU profile, launcher artwork/save previews, texture management and guarded timing research. |
| `portablekit/` | Pinned framework submodule with local changes preserved by the committed patch. |
| `patches/portablekit-local.patch` | Complete framework changes; must match the working submodule diff. |
| `scripts/` | Build/install/generate/test/package workflow, diagnostics, research and texture composition. |
| `tests/` | Port regression fixtures; framework tests live in PortableKit. |
| `assets/branding/`, `assets/launcher/` | Original generated logo/background artwork and provenance notes. |
| `generated/`, `overlays/` | Local generated game corpus/overlay trees, ignored by Git. |
| `build/windows/` | Current development build, isolated installations and ignored evidence. |
| `build/windows/packages/` | Frozen candidate/release folders, ZIPs and checksums. |
| `baselines/2026-10-06-working-prototype/` | Preserved local prototype snapshot; not a remote disaster-recovery backup. |

PortableKit is pinned to **`8b0c3fedd1d908dc2d171f8257c5c3d9af8886aa`**.
Do not update it implicitly. Apply the saved patch using the documented workflow.
The patch was checked against pristine pinned sources and reverse application;
the complete working diff was also verified before the alpha checkpoint.

The supported disc is **USA ULUS10391**, volume label MHFU, disc version 1.01.
The installer uses the disc's plain `BOOT.BIN` ELF; do not assume it decrypted
the encrypted `EBOOT.BIN`. Supported ELF SHA-256:
`3070954e2dc88fd0b0836c0c1dc526531aca4a213b0f18806e324963efb514f4`.
Other regions and modified executables are not established as compatible.

Windows workflow: Patch → Configure/Bootstrap → Install → Generate → Build →
Test → Package, with overlay recovery/generation as documented. Current standard
builds keep `MHFU_NATIVE_60_EXPERIMENT` and action timing diagnostics off.
Packaging rejects stale sources, changed binaries/overlay sets and research
options in alpha packages. Build records include source and binary hashes.
See [BUILDING.md](BUILDING.md) and [DISC.md](DISC.md).

The first clean workflow verification had one transient texture-pack directory-
swap test failure; targeted and full reruns passed, with no established root
cause. Preserve that history rather than describing the first attempt as clean.

## 4. Work completed and setbacks

Dates below follow the recorded local investigations, mainly 6–11 October 2026.
This is an outcome history, not a claim that every work item began on those dates.

### Working prototype and reproducible build

The owner initially verified loading a hunter, village movement, a hunt,
completion, saving, returning, restarting and a second hunt. Four different
executables were found locally; their identity was not assumed to be identical.
They and source/build records were preserved separately with a 548-file verified
baseline. No original save, game image or user setting was copied into that
baseline. A separate Windows build workflow was established and checked.

### Missing native startup code and overlays

Strict runs with interpreter fallback disabled exposed missing startup code
embedded in `.data`. Seven constructors were identified and compiled using the
extra range `0x089B93D8–0x089BAB70`. Boot then advanced into runtime-loaded code.

Native coverage expanded through boot, character editor, character creation and
save, continuation/cinematic, village, house movement, dialogue and quest entry.
Missing task/stage/monster libraries were captured and compiled incrementally.

The owner bought and equipped armor and a weapon, entered **Slay Giaprey**, and
the run stopped after several kills. Its log identified missing **stage094**,
rather than establishing a general combat defect. Compiling that library brought
the package to 19 libraries; the owner then completed the quest successfully.
Logs corroborated native transitions, successful saving and normal closure.

An executable inventory declared 299 sections: 298 game overlays plus a debug
declaration. Offline archive research recovered all 298 game overlay images
from `DATA.BIN`; the declared `debug.elf` was not recovered. All 279 remaining
game libraries compiled in ten batches with two jobs. Registration metadata and
identity checks passed. The archive contained 6,657 entries, but that research
was not a complete texture-art inventory.

**Limit:** every library is built; every library's gameplay has not been played.

### Expanded-build startup stall

After adding the complete library set, the owner reported a window opening with
no game. Headless reproduction timed out before useful output. Eager startup
loading of all DLLs was replaced with indexing canonical filenames and loading
matching overlays on demand, retaining ABI/export/corpus checks. Boot returned
to roughly five seconds in the bounded headless check. The owner subsequently
loaded a save, finished a quest, saved and returned successfully.

The helper also had a diagnostic bug: an unavailable exit code was incorrectly
formatted as zero. Retaining the process handle and preserving null when needed
corrected the record. The original evidence was kept.

### Jungle slowdown, sound gaps and delayed controls

The owner's broad test found starting area, 4, 8 and 10 smooth; areas 1 and 2
were worst, with several other areas affected. Slow stretches showed CPU geometry
preparation around 36–38 ms and CPU rendering around 41 ms, while GPU time was
typically under 1 ms. Audio/input symptoms accompanied the slowdown.

GPU vertex decoding improved some speed but corrupted models, especially trees;
it remains disabled in the supported path. An SSE2 CPU skinning optimization
matched the scalar reference and improved synthetic skinning benchmarks, but
did not resolve the real Jungle workload by itself.

Additional instrumentation showed wasted decoding across wide index spans.
The final optimization decodes **only referenced vertices in sparse indexed
draws**. Representative geometry-preparation samples dropped to about 3 ms;
these were not identical-camera before/after benchmarks. Decoder and indexed
display-list comparisons each cover 23,040 synthetic cases against the reference.
The owner then reported all Jungle areas smooth. The FPS counter was enabled
at the top-left; **F3** toggles it. Brief transition dips and other-map behavior
still require coverage. Audio quality outside tested scenes remains unverified.

### Fullscreen and launcher redesign

Leaving fullscreen previously retained an oversized window. Explicit restoration
of cached window geometry after the SDL transition fixed the tested round trip:
960×544 → 1920×1080 → 960×544. Multiple monitors, DPI, maximized windows and
repeated in-hunt changes remain coverage work.

The owner rejected the original brown/orange style, selected the third proposed
direction, requested a brighter background, then changed teal accents to green.
The resulting launcher uses original mountain-lake scenery and green controls.
The chosen project name is **Freedom Unbound**. The first logo included a dragon;
the approved second logo removes it and retains the mountain/forest crest and
green/silver lettering. Original assets are embedded for portable distribution.

The logo was moved above the home content. Play moved out of the left menu and
sits beside three save cards. Settings, Mods, Saves, Game data and Quit remain
in the sidebar. Navigation and return focus were checked with native captures.

### Save preview discovery and implementation

The requested cards display hunter name, Hunter Rank, Zenny and saved play time
for all three slots. The reader is read-only and validates both PSP save integrity
and the game's inner encoding/checksum. Empty and unavailable slots are distinct;
preview failure does not block Play. It supports the measured USA format only.

Slot stride is `0x6B100`; decoded fields include name at `0x1000`, occupied marker
at `0x1012`, play time at `0x4FC8`, rank at `0x501C`, and Zenny at `0x6A250`.
These were independently checked against the supported program. Research projects
were format references, not copied implementations. Synthetic tests cover three
slots, encoding, corruption/truncation/oversize rejection, encryption round trips
and unchanged input hashes. See [SAVE_PREVIEW.md](SAVE_PREVIEW.md).

### Alpha preparation and dedicated executable

Automated checks covered installation into empty isolated data, invalid-image
rejection, ISO drag-and-drop setup, launcher navigation, fullscreen, copied-save
backup/export, logging and extracted package startup. File-browser navigation
was not exhaustively tested by the drag-and-drop check.

The logging script initially failed using `Get-FileHash` in an inherited Windows
PowerShell environment. Direct .NET SHA-256 fixed that compatibility issue.
Tests now isolate inherited game/research switches. One earlier regression
failure was caused by inherited `PSPRECOMP_NO_INTERPRETER=1`; clearing/restoring
test overrides resolved the environment defect.

The owner completed the 30 FPS acceptance loop, adding mining and consumable
coverage, then reloaded and completed a second loop with interpolation. Successful
save writes/loads were logged. The known interpolation house glitch was reproduced.

Alpha.1 was committed, packaged, audited and published privately. Alpha.2 added
**`FreedomUnbound.exe`**, a Windows GUI entry point that runs the existing logging
helper using built-in PowerShell without a console window. The helper still starts
the native game's branded launcher. Session failure displays an error dialog;
startup output is retained in `launcher-startup.log`. The `.cmd` entry remains
a fallback. The dedicated helper uses a static MSVC runtime; the game still
requires the documented Visual C++ x64 runtime.

Testing alpha.2 exposed a normal setup cancellation being returned as an error.
The framework exit handling was corrected; empty-data setup now opens/closes
normally. The corrected build passed 25/25 tests, dependency/source audits,
322 manifest entries, ZIP integrity, executable startup from paths with spaces
and an apostrophe, incomplete-package rejection and unchanged copied saves.
The package was republished as alpha.2; alpha.1 and earlier test evidence remain
preserved. GitHub's temporary draft tag was corrected to the intended version tag.

## 5. Frame-rate research: two distinct features

### Interpolation: available, experimental

Simulation stays at 30 updates per second; the renderer presents intermediate
frames. This can improve visual smoothness without advancing gameplay faster.
The owner liked its appearance at increased internal resolution.

The house background can distort while beds/item boxes remain visually separate.
Selecting 30 FPS avoids the observed issue. Settings can display
`60 (running at 30)` when the presentation RateGovernor falls back. Logged changes
include 60 → 45 → 60 and later 60 → 45 → 30 due to late presents, not a proven
house-specific protection. Selecting 30 then 60 resets the ladder and reactivates
the distorted interpolation. Both the underlying visual cause and scene-specific
pacing still need investigation. Release documentation labels this experimental.
An update is planned without a promised date.

### Native 60 FPS simulation: incomplete and deferred

The original game wakes its main update on a two-vblank cadence: approximately
60 callbacks, 30 wakes and 30 flips per real second in measured menu and hunt
sessions. Raising this to 60 logical updates initially sped up menus, village,
movement and other behavior. Native PC compilation alone does not change this.

Guarded experiments and probes investigated animation rate, root motion, blend
phases, input repeat, fades, quest clocks, announcement holds/slides and pause
behavior. Some targeted menu/timer comparisons were corrected and verified.
Those local successes never established a complete correct native 60 FPS mode.

The owner's later test found fast climbing/jumping/falling, premature gathering
and carving awards, fast NPC/quest HUD effects, and visible swings failing to
register expected hits/damage. This was **not merely a failure to initiate attacks**.
Paired 30/60 runs followed the same sequence: quest entry, starting-area ledge
jump, Popo kill/two carves, short/high climbs, running/drop and herb gathering.

Measured high-ledge flight took about 1.101 seconds at reference versus 0.551
at native 60; per-update gravity was still unchanged. Some overall action-state
durations were close, but that did not validate animation phases or item awards.
The owner clarified that reference carving gives the item after the first knife
motion, whereas native 60 awards it at that motion's start. Reward triggers need
their own trace. Killing a Popo proves some attacks damage it, not that all
expected contacts registered. Existing traces do not establish monster HP changes,
accepted collisions or individual damage application.

A PC crash interrupted this research and the work resumed from retained files
and logs. No cause tying that computer crash to the game was established.

Future research must map each simulation consumer/event, preserve 30 FPS as the
reference, fix flight integration and reward timing, then observe collision,
hit-stop and damage explicitly. Do not ship a frame-gate-only patch as native 60.
The research scripts use build-directory copies and signature/label checks;
they must not edit the original generated corpus in place. Research remains
separate from shipping builds. See [NATIVE_60FPS.md](NATIVE_60FPS.md) and
[NATIVE_60FPS_PLAYTEST.md](NATIVE_60FPS_PLAYTEST.md).

## 6. Yakumo comparison findings

The owner requested analysis of the installed
`F:\Games\yakumo-0.6.0-alpha.4-windows-x86_64` because Portable 3rd's dense maps
ran well. Its exact release source was checked at commit
`692e9b29cecd7cbbba95d5b0defaa6d78a2a797e`.

For graphics, the comparison found shared decoder architecture and an opt-in
GPU path; it did not supply a default setting that magically corrected FU.
FU-specific workload measurements led to the sparse-index fix instead.

For frame rate, the inspected Yakumo release keeps simulation at **30 FPS** and
uses presentation interpolation for 60. It blends matched transforms/bones or
decoded vertices, holds orthographic UI, and guards discontinuities. FU already
inherits this architecture. It is not a native-60 animation/event correction
that can be copied into FU. This was source inspection, not a controlled Yakumo
runtime benchmark. References are pinned in the native research document.

## 7. Mod studies and requested bundles

**Studies complete enough to guide investigation; gameplay implementation is
paused. No bundled HD, camera, QoL or controller-HUD mod ships in the alpha.**

| Bundle | Requested scope | Remaining discovery/implementation |
| --- | --- | --- |
| HD textures | Faithful higher-resolution artwork. Existing packs are reference only. | Catalog originals/fingerprints, author original assets, verify seams/palettes/atlases and map/equipment/monster coverage, profile memory/loading. |
| Modern camera | Smooth right-stick control and press-L monster targeting. | FU camera ownership/modes/angles, actor identity/lifetime, target selection, L branch and aiming behavior. Preserve held-L items and original recenter if no target. |
| QoL indicators | Question mark above the hunter at a usable gathering spot; knife near a carveable corpse. | Actual interaction eligibility/range, exhausted nodes, remaining carves and world-space placement; proximity alone is insufficient. |
| QoL corpse time | Longer corpse despawn timer. | Lifecycle counter, area unload/quest cleanup and respawn/progression exceptions; configurable duration. |
| QoL completion timer | Visible return-to-village countdown. | Read the real countdown for relevant quest endings without changing it. Native timing research is a lead, not a shipped display. |
| QoL solo scaling | Multiplayer/guild quests scaled for one hunter. | Agree a formula, map quest/monster modifiers and solo conditions, address host/join rules. Do not assume simply halving HP is faithful. |
| Controller/keyboard HUD | PS5, Xbox, Switch Pro, keyboard and mouse. | Inventory guest HUD/menu/tutorial prompts; fixed presets versus dynamic bindings, device switching, combinations and texture conflicts. |

### Texture foundation already implemented

The runtime identifies original textures and replaces matching ones with PNGs
mapped by `textures.ini`; missing art falls back to the original. The Mods page
supports import and persistent replacement on/off. A separate tool composes
compatible flat HD/HUD packs with deterministic later-layer priority,
deduplication, disabled-key fallback, validation and preservation of originals.
Six synthetic-art cases and an importer fixture establish format integration,
not a completed HD pack or broad visual coverage.

One effective pack is selected at runtime. Automatic composition of independent
launcher bundles and broader INI features remain future work. PNG is supported;
KTX2 support must not be assumed. The loader's 8,192-pixel side limit and default
1 GiB replacement-cache budget are ceilings/settings, not artwork targets or
minimum hardware requirements. A 4× increase in each dimension means 16× pixels.
Texture dumping only captures visited content and can skip work when its queue
fills; repeat collection and a scene inventory are necessary.

### Proposed behavior-mod architecture

Use independently switchable, compiled FU feature modules and a validated game-
state adapter first, rather than an arbitrary drop-in DLL loader. Keep generic
presentation/input/cache infrastructure in PortableKit and FU addresses/behavior
in the port. Check supported executable identity and discard actor/camera state
across overlay changes, loads and transitions. Proposed module names in the plan
are design suggestions, not evidence that those files/features exist.

Artwork manifests should record content provenance, compatibility, conflicts and
hashes. Preferences remain separate from saves. Apply lifecycle/scaling changes
at a safe point or before a quest; toggles must fall back to the original game.

Host menus already identify controller families; this does **not** mean guest
game HUD prompts match them. FU currently maps right stick to PSP D-pad and has
no configured smooth native camera driver. Generated intra-unit calls can bypass
outer dispatch hooks, so each required native interception must be validated.

### Existing mods and community-request study

References included K's HD, Three5media, Alien038, Gathering Nodes, MHFU Target
Camera, PSP camera patch lite, FUComplete and Shared Urgents. Findings and links
are in [MOD_CASE_STUDY.md](MOD_CASE_STUDY.md). No reference pack was installed or
bundled during the study. Regional code/addresses are not automatically USA-
compatible; FUComplete's Japanese basis matters. KTX2 artwork is not a PNG pack.
Reuse permissions must be established before distributing external art/code.

Community discussions support interest in modern camera and convenience features,
but are not a representative survey or a reliable solo-scaling formula. Exact
matching implementations for every requested QoL feature were not verified;
that search result does not prove none exists.

Planning details: [MOD_IMPLEMENTATION_PLAN.md](MOD_IMPLEMENTATION_PLAN.md),
[TEXTURE_PACK_WORKFLOW.md](TEXTURE_PACK_WORKFLOW.md).

## 8. Dedicated cheat menu study

PSP/CWCheat support is feasible only after classifying each code. **No generic
cheat engine ships in alpha.2.** On 11 October the owner authorized native
unlimited-health/stamina and one-hit-kill controls to accelerate progression
testing. Local implementation adds launcher/in-game controls and guarded AOT
hooks. Standard builds and 27 regression checks pass. Launcher/in-game controls,
quest startup, runtime stamina refills and actual sprint-cost suppression were
observed using copies. The owner subsequently confirmed all three combat cheats work perfectly and authorized max money, no crafting/upgrade materials and max stacks on item-box deposit. See [CHEATS.md](CHEATS.md)
for provenance and validation. The owner then confirmed crafting several armor sets and weapons, upgrading weapons, money staying at maximum without decreasing, and deposited items multiplying correctly. The owner also confirmed disabling cheats followed by a reset restores original behavior. After completing urgent Khezu, the owner authorized progression-aware forging: the revised armor hook preserves hidden recipes and bypasses materials only for visible forge recipes, retaining upgrade behavior and original weapon unlock checks. Package `FreedomUnite-20261011-060304` passes its audit and the persistent playtest helper selects it. Revised cheat fixtures pass; texture-pack cancellation and damaged-save fixtures each failed once across two full-suite runs and passed individual reruns. Live post-Khezu forge-list comparison and exhaustive equipment coverage remain acceptance work. Frozen release packages are unchanged.

Latest follow-up: the owner accepted version 3's weapon lists but reported that
Create Armor still exposed all recipes. The second Create-list caller had been
misclassified as an upgrade caller. Version 4 corrects that caller while keeping
actual upgrade hooks unchanged. Package `FreedomUnite-20261011-061402` includes
the new executable icon, passes its audit and 27/27 regression targets in one
run. The playtest helper selects it and preserves the current hunter. Owner
acceptance of the corrected Create Armor list was subsequently received: the
owner confirmed it is fixed and accepted the new icon. The owner then reported
decorations still showed their full list. Version 5 applies the same original
visibility preservation to the decoration Create caller; upgrade behavior is
retained. Package `FreedomUnite-20261011-062443` passes its audit and 27/27
regression targets in one run; the helper selects it while preserving the
existing hunter. The owner subsequently confirmed the decoration crafting list
works as intended, but Attach still exposed all decorations. Version 6 preserves
the original quantity check for Attach and its normal jewel deduction, retaining
the free crafting material bypass. All 27/27 regression targets passed in one
run; the owner subsequently confirmed Attach works fine. Version 7 adds the
requested L+R quest-success shortcut and a stable branded desktop shortcut.
The shortcut passes 27/27 regressions and a bounded native copied-hunter run
reached Quest Cleared and normal rewards for Mountain Herb Picking. That check
used unthrottled native 30 and audio disabled; both saves remained unchanged.
Package `FreedomUnite-20261011-065628` passes its manifest/dependency/freshness
audit and the stable branded desktop shortcut selects it through the persistent
helper. Quest-type and saved progression acceptance remain owner playtests.
See CHEATS.md for package and
audit evidence.

- RAM data writes may work when native functions still read those guest values;
  addresses, region, field widths, bounds and lifecycle need validation.
- Instruction patches do not automatically alter ahead-of-time compiled C++.
  Frame-gate/quest-timer patches need compiled equivalents or targeted generation.
- Conditional/pointer/button codes require their own command evaluator and PSP
  input semantics. They are not all simple write operations.
- Earlier money/stamina examples were research leads; the current native controls are separately authorized;
  differently sized writes to a field must not be accepted without checking it.
- Build a supported-USA compatibility boundary, independent controls, clear
  failure behavior and copied-save tests. Do not claim every emulator cheat works.
- Any implementation/reuse must follow dependency licensing; the study did not
  authorize copying an emulator cheat implementation into our framework.

Owner order: after first alpha, with stability work still taking priority.

## 9. Released artifacts and practical instructions

Last releases published by the agent:

| Version | Purpose | Access |
| --- | --- | --- |
| [v0.3.0-alpha.1](https://github.com/Dudux42/FreedomUnbound/releases/tag/v0.3.0-alpha.1) | Initial packaged alpha with logging shortcut. | Private prerelease. |
| [v0.3.0-alpha.2](https://github.com/Dudux42/FreedomUnbound/releases/tag/v0.3.0-alpha.2) | Dedicated executable launcher, updated instructions, normal first-run cancellation. | Private prerelease. |

The owner also mentioned manually adding a release; no further release/version
was verified in this record. A future agent should inspect remote releases before
creating another version, rather than assuming alpha.2 is still the newest.

Alpha.2 source checkpoint: **`3e47afb50ca898aab99815b589ea11113b68a172`** on
`codex/alpha-release`, pushed to origin. Current recorded remote default branch
is `codex/jungle-cpu-skinning`; the release branch and tags contain the updated
README/instructions. Do not assume the landing-page default branch contains all
release work. Branch/default-branch consolidation remains release housekeeping.

Alpha.2 ZIP: `build/windows/packages/FreedomUnbound-0.3.0-alpha.2-windows-x64.zip`.
Size: **108,209,587 bytes**. SHA-256:
`eecff7a0ad0f21dd9c3abacef47932a777b254e30225ab859e2d7a5809c881f7`.
Packaged game executable SHA-256:
`A2A548669C80A76F78968C20621B6CE29F057FAA4708837812F2AFC6A8E9EDE9`.
All 298 overlay DLLs match the previously manually tested candidate. Alpha.2
changed startup/version presentation and packaging, not combat/gameplay timing.
It was not subjected to another full manual hunt after those startup changes.

Download the Windows ZIP from release **Assets**, extract all files to a writable
folder, and run **`FreedomUnbound.exe`**. Keep `FreedomUniteNative.exe`, scripts,
DLLs and overlays together. The generated GitHub “Source code” download is not
the playable package. GitHub Packages is unnecessary for this ZIP distribution.
Invited testers need repository access; a private release link alone does not
grant access.

Published requirements: Windows x64, Vulkan-capable graphics/driver, AVX-capable
CPU and Microsoft Visual C++ x64 runtime. These are stated prerequisites, not
a measured minimum-performance specification. Test on weaker hardware before
claiming minimum/recommended specs. Supply an original supported USA ISO.

Use 30 FPS as the reference. Esc or L3+R3 opens the in-game menu; F3 toggles FPS.
Save import requires the game's key after boot and is unavailable in pre-boot
Saves. Back up original saves and honor any restart prompt. See
[ALPHA_PLAYTEST.md](ALPHA_PLAYTEST.md) and [RELEASE_NOTES.md](RELEASE_NOTES.md).

Session logs: `%LOCALAPPDATA%\FreedomUnbound\logs`; report `stdout.log`,
`stderr.log`, `session.json`, hardware/settings and reproduction steps. For
failure before a session, include `launcher-startup.log`. Review personal paths
before sharing; do not upload game images/assets.

## 10. Known issues and limits still open

| Priority | Issue or evidence gap | Required next evidence |
| --- | --- | --- |
| High | No complete original single-player progression pass. | Quest/progression matrix and start-to-end runs with cold save/reload checkpoints. |
| High | Hardware coverage limited to the owner's desktop. | Weaker laptop setup/hunt/audio/input/save/reload report; then additional GPUs/drivers. |
| High | Save reliability beyond tested paths. | Multi-slot, import, longer play, progression, interruption/error recovery and migration tests on copies. |
| Medium | House interpolation distortion and presentation fallback. | Reproducible scene captures/traces; targeted correction and tests, or keep experimental/fallback. |
| Medium | Brief transition hitches and broad audio correctness. | Annotated transitions, other dense maps, music/effects/cutscenes and long-session traces. |
| Medium | Input/display edge cases. | Device variants/rebinding, hotplug, DPI/multiple displays, resized/maximized/fullscreen cases. |
| Deferred | GPU vertex decoding corrupts geometry. | Correct format/skin handling before enabling; stable CPU path remains reference. |
| Deferred | Native 60 movement, animation/event/HUD and contact timing. | Paired real-time traces including damage/HP/contact probes and owner combat validation. |
| Unverified | Multiplayer and every content library. | Separate multiplayer assessment; actual quest/map/monster/weapon execution coverage. |
| In development | Seven native progression-testing cheats; owner accepted the six earlier controls and owned-only Attach. L+R completion passes 27/27 checks and an isolated gathering quest reached Quest Cleared and rewards. | Owner quest-type, normal-paced and saved progression checks; broader equipment coverage remains. |
| Planned | Bundled mods and cheats beyond the six authorized controls. | Authorization, state discovery, implementation and feature-specific acceptance checks. |

The owner confirmed certain consumables; do not generalize that to every item.
No-crash boot results do not validate combat. Low GPU use does not establish all
future bottlenecks are CPU decoding. Local dependency audits do not prove driver
or runtime compatibility on another computer.

## 11. Milestones and what remains for beta

The owner's milestone definitions are the reference:

| Stage | Definition | Our position |
| --- | --- | --- |
| Pre-alpha, v0.1 | Launch, graphics, menus and some movement. | Passed. |
| Alpha, v0.3 | Basic hunting/combat/items/transitions/saving; major gaps may remain. | Achieved for the tested loop; private alpha.2 published. |
| Beta, v0.8 | Complete original single-player progression, all weapon types and reliable saving. | Not demonstrated yet. |
| RC, v1.0-rc1 | No known progression blockers; setup/controllers/intended features validated. | Future gate. |
| Full release, v1.0 | Faithful, stable native PC experience across intended original content. | Future gate. |

Beta acceptance checklist — **unchecked means not established by current evidence**:

- [ ] Define the complete single-player content scope: village and intended
  offline gathering-hall ranks/urgents, unlocks, endings and post-ending content.
  Do not equate completing one hunt with finishing original progression.
- [ ] Play through that scope from a new hunter with progression checkpoints,
  unlock verification and cold reloads; track blockers by exact quest/build.
- [ ] Cover every weapon: great sword, long sword, sword/shield, dual blades,
  hammer, hunting horn, lance, gunlance, bow, light bowgun and heavy bowgun.
  Exercise combos, hit registration, guarding where applicable, stamina,
  sharpness, ammo/coatings, reloads, damage/status and weapon-specific mechanics.
- [ ] Cover all intended maps/area transitions, representative monsters and
  variants, quest types, objectives, failure/abandon/retry, rewards and unlocks.
- [ ] Cover shops, equipment forging/upgrading, storage, inventory limits,
  item combinations and progression-dependent systems such as the farm/Felynes.
- [ ] Expand item coverage beyond tested consumables: gathering tools,
  traps/bombs, status effects/cures and quest-specific items; confirm consumption,
  effects, rewards and inventory persistence rather than only animation.
- [ ] Establish reliable saves across three slots, new/existing hunters,
  compatible imports/exports/backups, longer sessions and progression milestones.
  Validate error handling using copies; never use the owner's only save.
- [ ] Complete the weaker-laptop test and a documented hardware compatibility
  matrix, including installation on a machine without development tools.
- [ ] Check dense maps, transitions and long-session memory/resource behavior;
  confirm audio/input remain responsive and no broad timing regressions exist.
- [ ] Validate original 30 FPS animation, physics, quest clocks, menus and combat
  against expected behavior. Keep unvalidated native 60 hooks excluded.
- [ ] Resolve reproducible base-game crashes and progression blockers discovered
  by testers, with retained reproduction data and appropriate regression checks.
- [ ] Validate required controller families and keyboard/mouse, binding changes,
  disconnect/reconnect, fullscreen/window/display behavior and launcher focus.
- [ ] Validate first-run ISO selection, invalid/missing/moved image handling,
  clean upgrades, save/settings retention and usable startup error reporting.
- [ ] Keep release notes, instructions, branch/tag/source provenance, package
  manifest, dependency notices and checksum aligned with the actual beta build.
- [ ] Retest optional interpolation sufficiently to repair its known defect or
  retain it explicitly experimental and outside the beta's fidelity guarantee.

**Native 60 FPS, mods, cheats and multiplayer are not automatic beta blockers**
under the owner's single-player milestone. They become gates if the intended
beta explicitly promises them. Do not expand scope silently to postpone beta.

## 12. What remains for RC and full release

Full release requires the beta evidence above, followed by a stability and
compatibility pass rather than merely renaming an alpha package.

- [ ] Close all known progression-blocking issues in the intended content scope
  and validate fixes on a pinned release candidate.
- [ ] Confirm faithful original timing, combat, rewards, graphics, sound and
  cinematics across that scope, documenting any intentional differences.
- [ ] Complete repeated progression/reload and long-session testing; investigate
  crashes, save failures, leaks and resource exhaustion with actionable evidence.
- [ ] Validate clean installation/update across the supported Windows/hardware
  matrix and publish measured minimum/recommended requirements and limitations.
- [ ] Establish reliable save/settings preservation between versions and usable
  recovery/backup guidance; do not promise unsupported region conversion.
- [ ] Validate the shipping control/display/accessibility options and document
  device-specific limitations instead of assuming all controllers behave alike.
- [ ] Resolve the house interpolation defect before treating interpolation as
  stable; otherwise keep it experimental/disabled or exclude it from the guarantee.
- [ ] Ship native 60 only if every timing/event/contact acceptance gate passes;
  retaining native 30 is valid if native 60 is not a promised v1.0 feature.
- [ ] Include only validated promised mods/cheats, with tested compatibility,
  safe toggles, provenance and clear interaction rules. Optional roadmap features
  need not delay a faithful base-game release unless committed to its scope.
- [ ] If multiplayer is promised, separately prove connection/session behavior,
  quests, synchronization, saving and join/disconnect handling; current evidence
  does not support that promise.
- [ ] Finish release engineering: clean-source build instructions, complete
  framework patch, reproducible provenance, package/ZIP checks, dependency notices,
  project-source licensing decision, no bundled game data, support/reporting path.
- [ ] Choose public versus private distribution explicitly. Before exposing
  source history, audit historical content/author metadata; the latest-commit
  game-data exclusion check was not a full historical privacy audit.
- [ ] Freeze and retest RC with no unresolved critical issues; publish honest
  known limitations, upgrade guidance and a rollback/support plan for v1.0.

## 13. Immediate work order after this handoff

1. Collect invited tester reports and the weaker-laptop result. Start at 30 FPS
   and 1× internal resolution; assess hunt/audio/input/save/reload before raising
   resolution or enabling interpolation. Record CPU, GPU, RAM, driver and settings.
2. Prioritize reproducible startup failures, crashes, save issues and progression
   blockers. Keep current release/prototype and original save copies intact.
3. Establish a content/weapon/progression test matrix; avoid asking the owner
   to repeat already-passed loops without a change or unresolved question.
4. Investigate house interpolation and remaining transition/audio issues using
   annotated scenes and paired traces; retain the stable CPU geometry path.
5. Consolidate release documentation/branches and verify the remote state,
   particularly any release manually created by the owner.
6. Resume cheats, mods or native 60 work only under the owner's scope decisions.
   When native 60 resumes, fix proven consumer/event failures before another
   general combat handoff. Code/timer measurements belong to the agent; feel,
   contact and weapon acceptance still need informed player testing.

## 14. Handoff evidence and maintenance

Start a fresh agent with this file plus [README.md](README.md),
[BUILDING.md](BUILDING.md), [GROUND_RULES.md](GROUND_RULES.md) and the relevant
investigation. This Markdown is the handoff record; it does not create persistent
knowledge outside the repository by itself.

Important local evidence (ignored, not remotely backed up by source Git):

- `build/windows/investigation/alpha2-release-final/result.json` — final ZIP,
  hashes, source checkpoint, isolated executable startup and copied-save checks.
- `alpha2-tests-corrected.log`, `alpha2-audit-corrected.json` — final 25/25 and
  dependency/source/322-entry manifest results.
- `alpha2-first-run-corrected.log`, `alpha2-first-run/` — empty-data setup,
  screenshots and corrected normal exit.
- `alpha-owner-playtest/logs/` — owner's candidate gameplay/save sessions;
  `alpha-owner-playtest/game/` contains the isolated hunter they updated.
- `native-60fps/` — paired traversal/action research, traces and protected test
  data; use the detailed research note to locate exact comparisons.
- `archive-research/`, `boot-failure-20261008/`, `jungle-cpu-skinning/` and the
  vertex-decoder investigation records — extraction, coverage and performance.
- Alpha.1, alpha.2, timestamped candidates, precheck packages and the original
  baseline remain preserved locally. Do not delete them casually.

GitHub helper JSON files are local operation records and may still say “draft”
or contain a temporary draft URL after successful publication. Verify GitHub
itself before treating those fields as current release state. Do not print or
commit credential-helper output. No credentials belong in this document.

Useful source checkpoints: `64695f5` initial alpha source; `eeb7ddd` dedicated
launcher/instructions; `ea146f4` launcher audit; `3e47afb` setup-exit fix and final
alpha.2 source. The framework remains locally modified intentionally, with its
complete changes in the committed root patch; this is not an unrecorded update
to the submodule commit. This master document is a later documentation addition,
not part of the frozen alpha.2 tag.

On future updates, change the snapshot date, current release/checkpoint and
checkbox evidence together. Preserve the distinction between **verified**,
**reported**, **unresolved**, and **planned** so later work does not repeat
investigations or accidentally ship a rejected experiment.

### Launcher equipment/sidebar refresh — 15 October 2026

Removed the redundant left heading, fitted all navigation without scrolling,
added brushed button faces, and anchored version/status and language flags near
the bottom. Play is above the full-width card row. Cards show Pokke/Guild Points,
six equipped items with original game slot icons below, and hunter information
on the left beside the rotatable model on the right. The saved feet-first armor order is mapped to the requested
head-to-feet display order and was compared with both in-game Status pages.
Native normal/maximized/restored, drag isolation, English/Portuguese and Load
checks passed on copied saves; save files stayed unchanged. Details and evidence
are in SAVE_PREVIEW.md and the ignored launcher-refresh investigation directory.
The stable desktop shortcut targets the rebuilt executable.

### Windows activation and file dialogs — 15 October 2026

The native window explicitly shows/restores and requests foreground activation
on startup, file drop, and native picker return. The packaged GUI/PowerShell
logging chain passes foreground permission to its child. Startup is verified
from both Hidden and Minimized process launch modes: visible, restored and
foreground. A minimized launcher receiving a ZIP drop restores and foregrounds;
home-screen drops now reach the Mods review page rather than being discarded.

On Windows the shared file browser opens SDL's native Windows file/folder
dialog directly, with the launcher as owner and request-specific extension
filters. ISO file, save-import folder and save-export folder dialogs and cancel
paths were exercised. The save editor already uses native Win32 file selection.
Other platforms retain their existing browser. A harmless text-mod install was
verified only in the copied test profile. Save hashes stayed unchanged. Evidence:
`build/windows/investigation/window-dialogs/`. Developer-only overrides are
`MHFU_BACKGROUND_WINDOW=1` and `MHFU_INTERNAL_FILE_BROWSER=1`; normal launches
need neither. The stable desktop shortcut continues to target the rebuilt exe.

### Save-card equipment icon follow-up — 15 October 2026

Removed equipment/info hover popups. The launcher now reads exact weapon-class
and armor-slot sprites and rarity colors from locally derived original game
tables, displaying full 24×24 rectangles rather than cropped icons. Both atlas
pages are supported; texture observation preserves GPU vertex decoding.
Original-code verification passed 31 equipment samples across 11 weapon classes
and 10 rarities. Native normal/maximize/restore and hover checks passed, with
unchanged save hashes. Stable exe and owner-derived icon caches are updated.
See `SAVE_PREVIEW.md` for cache details and ignored validation evidence.

### FreedomUnbound-test packaging — 15 October 2026

The owner requested a fresh Windows test package and a GitHub source update.
`TEST_BUILD_CHANGES.md` records launcher/progression changes, excluding the
standalone save editor; README includes the same update summary. The configured
Git remote is `Dudux42/FreedomUnboundTest`; this supersedes the old remote note
for this checkout. Runtime binaries belong in a private release asset, while
source, tests and the complete framework patch belong in Git. Personal saves,
original game data and derived preview caches are excluded from distribution.

Package validation: the standard release build and all 52 CTest targets passed
in the confirmation run; the save-preview fixture also passed with the local
save table. The initial suite had one save-editor backup fixture failure; its
targeted rerun and the full confirmation passed without changes. The cause of
the first failure is not established. The editor is excluded from this package.
The packaged GUI opened the home screen in a bounded copied-profile smoke test
(exit 0, roughly six seconds); its capture was inspected and save hashes stayed
unchanged. Evidence: `build/windows/investigation/launcher-refresh/package-*`.
