# Bundled mod implementation plan

Updated 9 October 2026. This is an implementation design derived from the local
source audit. None of these gameplay mods is enabled in the current build.
The initial texture-management foundation is now implemented: a Mods page with
persisted replacement selection/import/original fallback, plus a tested flat-pack
composition tool. See [TEXTURE_PACK_WORKFLOW.md](TEXTURE_PACK_WORKFLOW.md).
The supported game is USA ULUS10391; the profile's executable hash is the
compatibility boundary. The preserved 30 FPS package remains the reference.

## Architecture and ownership

Use compiled, independently switchable feature modules in the FU profile for
the initial bundled release. Do not start with an arbitrary DLL plugin loader.
Artwork is external data; behavior runs through our own compiled modules.
The generic framework owns launcher presentation, input, texture replacement,
and UI drawing. All FU addresses, camera modes, actors, quests, and native
function corrections belong to this repository's `host/` integration.

Suggested future modules: `mod_config`, `game_state`, `camera_mod`, `qol_mod`,
and `hud_prompts`. These are proposed source files, not files already created.
The adapter exposes validated snapshots of quest mode, player, camera, live
large monsters, interaction eligibility, and timers. Feature modules never
retain raw actor pointers across overlay changes, quest changes, save loads,
or scene transitions. Invalid state disables that feature until valid again.

A proposed manifest identifies bundle ID/version, supported disc and executable
hash, feature IDs, content hashes, required modules, conflicts, default-off
state, and asset provenance. A manifest selects registered features; it does
not execute code or contain unchecked address writes. Store user preferences
separately from packaged defaults. Save files must not contain host mod settings.

The launcher now has texture-management Mods controls and persisted selection.
Gameplay compatibility status, feature selection, and a complete Original preset
still need implementation. Apply gameplay toggles before boot or at the village; an
active hunt should defer timing/lifecycle/scaling changes until the next quest.
Texture changes can use existing reload machinery after safe cache refresh.
Report a failed feature without preventing the unmodified game from launching.

## Integration constraints established by the source

- `GameProfile::camera` and `CameraDriver::frame` already provide a per-real-game-
  frame callback. FU currently has no driver and maps right stick to D-pad.
- Camera input already integrates held rates over elapsed time and takes mouse
  motion once. Use it at both 30 and future 60 FPS; do not multiply stick input
  by an assumed fixed frame count.
- `VulkanRenderer::pad()` exposes PSP button state. A robust targeting action
  needs a game-thread input edge or the native L-camera branch, not UI-only
  controller polling that can repeat or miss a press.
- A frame callback at presentation can prepare the next frame, but the exact
  camera update stage must be mapped before assigning guest camera angles.
- Generated intra-unit calls bypass outer-dispatch hooks. RAM instruction
  edits alone do not alter compiled behavior. Required routine changes need
  an explicit compiled interception or repeatable targeted regeneration.
- Current texture selection produces one effective pack. HD and HUD composition
  must be implemented before promising independently stackable texture mods.
- No actor/interaction/scaling adapter or general world-space icon layer is
  implemented. Framework support is a foundation, not completed FU behavior.

## Modern camera, including L targeting

Both smooth right-stick control and press-L monster targeting are required.
Start with a snap on the rising edge of the PSP L action, whichever device is
bound to it. Continuous tracking is a separate future option. Holding the
button must continue to support original item selection and combinations.

Proposed behavior:

1. Build a validated list of living large monsters in the current area. Exclude
   despawning actors, corpses, unrelated small creatures, and other-area actors.
2. Keep the selected monster through occlusion while it remains a valid same-
   area actor. Identify it by validated actor identity/generation, not just RAM
   address. If selection disappears, choose a valid replacement; if none exists,
   retain the original player-facing recenter behavior.
3. For an initial implementation, automatically choose the nearest eligible
   large monster when no target is selected. Provide an independently bindable
   Cycle target action for multi-monster quests; do not assign an existing
   gameplay button combination without an explicit control design.
4. When L is pressed in a controllable gameplay camera, turn toward the target's
   validated aim point. Derive yaw/pitch using the game's coordinate convention
   and angle units, then clamp its vertical limits. Preserve camera distance,
   collision handling, and movement-relative orientation behavior.
5. The original L recenter routine and our target turn must not compete. Map that
   native branch or a suitable input/update hook. Preserve the rest of the PSP
   L state for item controls; suppress only the conflicting camera operation.
6. Disable snap and discard queued camera motion in menus, cinematics, loading,
   and unverified aiming modes. Bow/bowgun scopes need separate behavior checks.
   Ordinary right-stick movement resumes immediately after a snap.

Unknowns requiring observation: camera state ownership, angle units, mode flags,
actor list/lifetime, target height/aim point, native L branch, and selection UI.
Complexity is high because targeting adds game-state discovery to analog camera
discovery. No regional addresses from an external mod should be assumed valid.

The [existing target-camera project](https://github.com/pggkun/mhfu_target_camera)
is a behavior and research reference. No source or patch was copied or installed.

Acceptance checks: one and multiple large monsters; enemy behind the hunter;
enemy off-screen, dead, or leaving; no target; slopes; walls; held L with item
selection; mouse/keyboard rebinding; weapon aiming; scene transitions; 30/60 FPS.

## HD textures and controller HUD

The implemented replacement loader accepts PNGs plus `textures.ini`, and can
dump originals. First collect a scene-indexed inventory, then author a small
original-art vertical slice: several village textures and one HUD sheet.
Preserve UV layout, alpha edges, palette-related hash matching, and seams.
Existing packs remain reference only. Do not add KTX2 artwork to a PNG-only loader.

Implement deterministic composition into a separate effective pack: HD base,
selected controller HUD override, then explicit user override. Verify compatible
hash algorithms/options before merging. Detect conflicting hashes, keep source
packs immutable, record which bundle supplies each replacement, and rebuild the
effective mapping/cache when selection changes. Missing art uses original art.
Use an initial conservative memory budget and compare cold/warm loading and
GPU memory pressure. Texture size is not a geometry-detail upgrade.

HUD variants: PS5, Xbox, Switch Pro, keyboard/mouse, and Original. Match prompts
to physical/action mappings, especially Nintendo versus Xbox letter positions.
Start with documented presets. Binding-aware keyboard prompts require generated
labels or a host overlay, respecting both configured bindings and available HUD
space. Some tutorials may use text rather than texture glyphs; inventory them.
Automatic device switching needs a manual override and stable device-activity
selection so incidental input does not repeatedly rebuild the pack.

## Quality of life

| Feature | Required discovery/integration | First validation |
| --- | --- | --- |
| Gathering question mark | Actual available interaction, distance, depletion, hunter screen projection | Accessible/depleted nodes; correct vertical placement and eligibility |
| Carving knife | Corpse state, remaining carves, range, carve eligibility | Alive/dead/depleted/despawning monsters; no false prompts |
| Longer corpse persistence | Corpse lifecycle and cleanup boundary | Bounded extension; unchanged quest/area cleanup and actor reuse |
| Return countdown | Actual remaining return timer and quest state | Completion, failure, early departure, paused/menu states |
| Solo Guildhall scaling | Quest class, participant count, base health/damage/stagger parameters | Apply once to eligible offline solo Guildhall quests; village unchanged |

Gather/carve icons need an independently drawn overlay projected above the
hunter, with resolution/aspect handling and no intrusive HUD overlap. A single
selection rule should avoid showing both prompts when the game can execute
only one interaction. Timers must use game state, not a guessed host countdown.
Corpse duration and scaling are gameplay changes; defer until identity/lifecycle
is mapped. Do not scale drops or rewards implicitly, repeatedly multiply existing
stats, or assume Village and Guildhall have interchangeable difficulty tables.
Initial solo scaling should be disabled in actual multiplayer sessions.

## Implementation order and test gates

1. Complete base diagnostics, fullscreen restoration checks, package integrity,
   and 30 FPS action measurements. Keep native-60 research separately optional.
2. Add feature settings and compatibility checks; prove every option defaults
   off and Original restores base behavior. A Mods page should advertise only
   implemented features, not placeholders presented as working mods.
3. Deliver the small HD/HUD composition trial, then expand authored coverage.
4. Map camera and actors, implement analog movement, then L snap and target
   selection. Validate ranged weapons and transitions before bundling.
5. Implement interaction icons and the measured return countdown.
6. Investigate corpse persistence and solo scaling with separate balance tests.

Build/tests can verify configuration, composition, geometry, imports, and bounded
startup. Human gameplay is still needed to establish combat timing, camera feel,
prompt usefulness, balance, and progression. Do not label these mods complete or
ship them enabled based only on successful compilation.
