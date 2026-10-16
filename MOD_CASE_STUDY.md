# Freedom Unite mod case study

Updated: 9 October 2026

## Scope and current status

This document records the proposed bundled mods, repository findings, online references, unresolved questions, and suggested implementation order. It is a planning document, not an implementation commitment.

The working prototype is preserved. The initial study did not implement mods;
the 9 October follow-up implements launcher texture-pack management and a
composition authoring tool. Bundled artwork and gameplay mods remain unfinished.
See [TEXTURE_PACK_WORKFLOW.md](TEXTURE_PACK_WORKFLOW.md) for supported scope.

Existing mods are research references only. The intended HD pack will use our own artwork rather than copy an existing pack.

## Proposed launcher bundles

| Bundle | Requested features |
| --- | --- |
| HD textures | Improved artwork while retaining Freedom Unite's visual identity. |
| Modern camera | Smooth right-stick rotation plus facing the selected large monster when pressing L. |
| Quality of life | Gathering question mark above the player; carving knife above the player; longer corpse persistence; quest-completion countdown until village return; solo scaling for multiplayer quests. |
| HUD input prompts | Button and key prompts for PS5/DualSense, Xbox, Switch Pro, and keyboard/mouse. |

Bundled means included and available to select. Each bundle should be optional, and each QoL feature should have its own toggle. Turning mods off should restore the base behavior.

## Existing project support

- The launcher provides Play, Settings, Mods, Saves, Game data, and Quit. Mods currently manages texture replacement/import; gameplay-feature selection, manifest discovery, and dependency handling remain future work.
- The renderer already supports texture replacement, texture dumping, pack importing, and enabling/disabling replacement.
- Texture packs currently use PNG images and a `textures.ini` mapping. The decoder is PNG-only; compressed formats such as KTX2 should not be assumed compatible.
- One effective texture pack is selected at a time. The new authoring tool composes compatible flat HD/HUD mappings with later-layer priority. Automatic launcher composition and broader INI formats remain future work.
- The framework has camera-input handling and an optional game-specific camera driver. Freedom Unite currently defaults to right-stick-to-D-pad mapping and has no configured native camera driver.
- Host UI widgets already identify controller families and draw appropriate labels, including PS5 and Switch Pro. This is host-menu support, not replacement of the game's HUD.
- Keyboard/mouse bindings expose readable names and support two bindings per action.
- No implemented gathering/carving indicators, corpse-persistence hooks, village-return countdown display, or solo monster-scaling feature were found in the inspected host code.
- Native gameplay changes need game-specific integration. PSP instruction patches or CWCheat codes do not automatically alter already compiled C++ functions. Data writes can be applicable where native code reads those values, but that must be investigated individually.
- HLE extensions are linked at build time. They are not an existing general-purpose drop-in gameplay DLL mod loader.
- Alternate executable/corpus support exists in the framework, but no alternate variant is configured for this game.

Relevant source files:

- [Launcher home](portablekit/host/ui/launcher_home.cpp)
- [Freedom Unite profile](host/freedom_unite_profile.cpp)
- [Profile hooks](portablekit/host/profile.hpp)
- [Texture replacement and dumping](portablekit/host/gpu/texture_pack.hpp)
- [Texture pack implementation](portablekit/host/gpu/texture_pack.cpp)
- [Renderer integration](portablekit/host/gpu/vulkan_renderer.cpp)
- [Texture importer](portablekit/host/gpu/texture_pack_import.hpp)
- [Replacement GPU images](portablekit/host/gpu/replacement_textures.cpp)
- [Camera input](portablekit/host/camera/camera_input.hpp)
- [Camera driver dispatch](portablekit/host/camera/camera_driver.hpp)
- [Input bindings](portablekit/host/input/bindings.hpp)
- [Host UI controller labels](portablekit/host/ui/widgets.cpp)
- [Windows packaging](scripts/build-windows.ps1)

## 1. HD texture mod

### How it works in plain English

A texture is an image covering a surface or providing an interface element. The game loads its original texture; our renderer identifies it; the replacement pack supplies an improved image; the renderer draws that image on the same surface.

This changes artwork, not model geometry, animations, or gameplay. Sharper monster skin will not by itself make a low-detail silhouette rounder.

The current game is the US release, `ULUS10391`. Game data remains in the player's disc image. Previous archive research identified 6,657 entries in `DATA.BIN` and extracted executable overlays, not a complete artwork inventory. We do not yet have a catalog of all textures.

The renderer can collect original textures as PNGs while the game is played. Each image receives a fingerprint based on its contents, dimensions, and palette where applicable. The pack maps those fingerprints to our replacement images. Missing replacements leave the original artwork in place; originals also remain visible while a replacement loads.

### Proposed authoring process

1. Agree on a faithful visual direction: original colors and designs, clearer materials, readable interface artwork.
2. In a future authorized collection session, use the existing texture dumper while visiting the village, maps, equipment previews, shops, and monsters.
3. Identify dumped images using screenshots and scene notes. Organize them into environments, village, monsters, equipment, effects, and UI.
4. Create original improved artwork from the game's own textures. Preserve proportions, transparency, seams, and the positions of pieces inside shared texture sheets.
5. Store replacement PNGs with readable names and connect them to original fingerprints in `textures.ini`.
6. Verify small batches in matched gameplay scenes, with replacement on and off. Check appearance, coverage, loading, and graphics-memory use.
7. Package the validated assets separately from base game data and integrate selection into the launcher later.

### Open issues and limits

- Runtime collection only captures textures encountered during play. One quest cannot establish complete coverage.
- The current dumper can skip textures when its queue is full, so repeat passes and an inventory checklist are necessary.
- A shared texture sheet may contain several unrelated pieces. Their layout must remain consistent with the game's use of that sheet.
- Palette variants and different texture fingerprints need careful matching. Broad matching rules must not replace unrelated images.
- Enlarging both dimensions fourfold produces sixteen times as many pixels. Use selective resolution increases rather than one maximum size for everything.
- Enlarging an image alone does not recover lost detail. Text, faces, patterns, and symbols need manual attention.
- PNG is the supported starting format. The current loader caps image sides at 8,192 pixels; this is a technical ceiling, not a target resolution.
- The GPU replacement cache has a configurable memory budget, currently defaulting to 1 GiB. This does not imply total game GPU memory usage or guarantee performance on every machine.
- Texture art and HUD button replacements can overlap; define how their selected assets combine.

Suggested first future trial: a few recognizable village surfaces and one simple interface image. This would verify the complete process before attempting thousands of assets. No textures were dumped or authored during this study.

## 2. Modern camera mod

### Requested behavior

Use the right stick for smooth camera rotation rather than merely simulate PSP directional-button presses.
Pressing L must also turn the camera directly toward a valid large monster.
This is a targeting snap on each press; continuous automatic tracking is not
part of the present request. Holding L must retain item-selection behavior.

### Findings and complexity

The framework already accepts analog camera input and can dispatch to a game-specific driver. Freedom Unite's current right-stick mapping provides original D-pad camera behavior. Completing the requested feature is a medium-to-high investigation task because the game's camera state and operating modes must be identified.

### Planned investigation

- Locate camera angles, rotation limits, and the routines that update them in the US executable.
- Determine behavior in normal combat, bow/bowgun aiming, scripted scenes, menus, and transitions.
- Integrate the existing camera-input framework with game-specific state and suppress competing original camera input when appropriate.
- Provide speed and inversion settings, suitable deadzone handling, and an original-camera fallback.
- Verify movement, aiming, and camera transitions without accumulating unwanted motion during menus or cutscenes.

Target-camera snapping and smooth analog rotation are both required parts of
this bundle. The independent implementation plan, including multiple monsters
and interaction with the original L behavior, is in [MOD_IMPLEMENTATION_PLAN.md](MOD_IMPLEMENTATION_PLAN.md).

## 3. Quality-of-life bundle

| Feature | Intended behavior | Investigation needed |
| --- | --- | --- |
| Gathering question mark | Show above the player when gathering is actually available. | Identify the interaction target, range, action availability, and exhausted-node state. |
| Carving knife | Show above the player when a nearby corpse can still be carved. | Identify corpse interaction eligibility and remaining carving opportunities. |
| Longer corpse persistence | Extend the time available before a corpse disappears; make duration configurable. | Separate normal despawn from area unloading, quest cleanup, and any progression-dependent removal. |
| Village-return countdown | Show the real remaining return time after quest completion. | Locate the actual countdown and verify different quest endings. |
| Solo scaling | Apply an agreed rule to multiplayer/guild quests played by one hunter. | Inspect quest modifiers, monster statistics, participant state, and host authority. |

### Design notes

- Indicators should follow actual action availability, not just proximity. Suppress them where interaction is unavailable, including relevant menus or scripted states.
- Gathering and carving may share an interaction-state detector, but their eligibility rules require separate validation.
- The countdown should display the game's true timer, not assume a fixed number of seconds. It should not change return timing.
- Corpse changes must account for quests that may depend on removal before spawning subsequent enemies.
- Do not assume halving monster health creates village-equivalent difficulty. Determine the intended treatment of health and other thresholds from game data and the desired experience.
- Apply solo scaling only under clearly defined solo conditions. Multiplayer behavior, participating-player requirements, and any join/disconnect rules must be established before enabling it in shared sessions.
- Decide whether gameplay modifiers can change during a quest or must be selected before departure. Applying them before a quest is the simpler initial design to investigate.
- Keep every QoL feature independently selectable.

Relative complexity: indicators and countdown require game-state discovery plus presentation; corpse persistence changes gameplay lifecycle; solo scaling requires the most balance and multiplayer investigation.

## 4. HUD input-prompt mod

### Requested coverage

PS5/DualSense, Xbox, Switch Pro, and keyboard/mouse. Prompts should describe the control that actually performs the action.

| Scope | Relative complexity |
| --- | --- |
| Fixed PlayStation and Xbox artwork | Low to moderate |
| Switch Pro labels matched to actual button positions | Moderate |
| Keyboard/mouse artwork for a fixed preset | Moderate |
| Prompts following custom bindings | Moderate to high |
| Automatic switching by active input device | Moderate additional work |

### Findings and planned design

The host interface already recognizes controller types and renders corresponding labels. The missing integration is inside the game's own HUD, menus, and instructions.

A proposed selector is Auto, PS5, Xbox, Switch Pro, Keyboard/Mouse, and Original PSP. Manual selection should remain available if device identification is ambiguous.

Fixed texture variants could support defined control presets. Fully binding-aware prompts require reading current input settings and producing labels that fit the game's interface. Handle two bindings per action, longer key names, mouse-button icons, and combined-button instructions.

Nintendo and Xbox lettering differs by physical position. Artwork must follow the actual mapping rather than a blind letter substitution. Changing prompt artwork does not itself remap controls.

Scope should include the quest HUD, menus, equipment screens, and tutorial instructions. Some instructions may be text rather than replaceable button images; their representation has not yet been inventoried.

The HUD mod and HD pack may replace the same texture sheets. Define explicit override priority or compose an effective pack without overwriting the user's base assets. Automatic switching also needs cache refresh and protection against rapid switching from incidental input.

## Online reference findings

These references establish precedents and possible research leads. No external pack or gameplay mod has been tested in our runtime.

| Reference | Relevant findings | Limits |
| --- | --- | --- |
| [K's HD](https://www.nexusmods.com/monsterhunterfreedomunite/mods/39) | Extensive artwork improvements; US/EU mappings; PlayStation, Xbox, and Nintendo HUD variants. | FU testing is described as basic. Uses KTX2, which our current PNG decoder does not support. Description does not establish dedicated PS5 or keyboard/mouse coverage. |
| [Three5media HD retexture](https://github.com/three5media/mfhu-hd-retexture) | Targets the US `ULUS10391` version; useful coverage and organization reference. | Not verified in our runtime; includes credited contributions from other packs. |
| [Alien038 MHFU-HD](https://github.com/alien038/MHFU-HD) | UI, screens, and item artwork reference. | Not verified in our runtime. |
| [Gathering Nodes by Lunng](https://www.nexusmods.com/monsterhunterfreedomunite/mods/26) | Cheat-based gathering indicators for US FU and Japanese/FUComplete. | Exact placement and eligibility behavior need inspection before comparing it to our requested question mark. |
| [MHFU Target Camera](https://github.com/pggkun/mhfu_target_camera) | US/Japanese support; turns camera toward a selected monster when activated. Public source can inform camera-state research. | Target snapping differs from smooth right-stick orbit. PSP cheat/patch integration is not automatically native-runtime integration. |
| [PSP camera patch lite](https://github.com/Freakler/psp-camera_patch_lite) | Lists Freedom Unite among supported titles; demonstrates right-stick input adaptation. | Simulated original controls should not be mistaken for a verified smooth analog camera implementation. |
| [FUComplete](https://fucomplete.github.io/) and [documented changes](https://fucomplete.github.io/docs/whats_changed.html) | Hold-to-gather/carve, faster supply pickup, field-of-view adjustment, minimap scaling, offline Guildhall equipment access, configuration, and file replacement. | Based on Japanese Portable 2nd G. Not a directly compatible US native mod package. |
| [Shared Urgents](https://www.nexusmods.com/monsterhunterfreedomunite/mods/14) | Shared gathering-hall urgent completion; updated code advertises broader PSP-game/region coverage. | Author notes incomplete testing across quests and versions. Outside the currently requested bundle scope. |

No clearly documented matching FU release was verified in this search for the exact carving-knife indicator, longer corpse timer, return-countdown display, or dedicated solo-scaling behavior. This is a search limitation, not proof that none exists.

### Community requests and feedback

- Players explicitly distinguish smooth camera motion from right-stick remapping of the original stepped vertical camera: [camera discussion](https://www.reddit.com/r/MHFU/comments/1tpxjek/decided_to_pass_time_with_mhfu_on_the_plane/).
- FUComplete release feedback praises camera, minimap, and supply-pickup improvements: [release discussion](https://www.reddit.com/r/MonsterHunter/comments/123ek6e/today_marks_the_15th_anniversary_for_portable_2nd/).
- Solo difficulty remains debated. Player discussions contain conflicting multiplier and balance claims; they should not define our implementation formula: [solo discussion](https://www.reddit.com/r/MonsterHunter/comments/1wwb19g/are_old_school_mh_games_single_player_friendly/).

These are examples of community interest, not a representative survey or technical verification.

### Reference and distribution boundaries

Existing mods remain references only under the current direction. K's HD explicitly prohibits repackaging or inclusion in another download without author permission. Gathering Nodes lists redistribution with creator credit under its conditions. These permissions do not change the decision to create our own HD artwork.

Public code may inform investigation, but any future reuse must comply with repository provenance and license rules. Region-specific addresses must be verified for our executable.

## Suggested future work order

1. Define HD art style and the initial small texture batch.
2. Collect and catalog originals in separately authorized sessions; validate the existing replacement path using our own assets.
3. Design HUD variants alongside the HD pack and establish how their assets combine.
4. Investigate Freedom Unite's camera state and implement smooth analog control when authorized.
5. Investigate interaction eligibility and the real return countdown; build presentation features when authorized.
6. Investigate corpse lifecycle and validate configurable persistence.
7. Establish solo-scaling rules from quest data, then validate solo and multiplayer boundaries.

Launcher design can proceed as a planning activity alongside these steps: package metadata, toggles, settings, game/build compatibility, dependencies, conflicts, and clear fallback behavior. Actual implementation should follow validated feature requirements.

## Outstanding decisions and verification

- HD artwork style, target resolutions, and initial scope.
- Complete texture coverage and identification of shared or changing texture sheets.
- Whether initial HUD support uses fixed presets or follows custom bindings.
- Input-device switching behavior and keyboard layout labeling.
- Camera addresses, modes, limits, and aiming behavior.
- Exact interaction-state and countdown sources.
- Corpse-persistence duration and quest-specific exceptions.
- Solo-scaling formula, affected statistics, and multiplayer activation rules.
- Effective-pack composition for HD textures and HUD overrides.
- Performance across target machines and visual correctness across maps, equipment, monsters, languages, and quest endings.

The existing gameplay-loop playtests establish a working base. They do not verify any proposed mod or imply complete game-content coverage.
