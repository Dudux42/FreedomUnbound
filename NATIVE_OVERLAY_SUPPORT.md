# Native overlay support

## Rendering

Gameplay mods reuse the existing ImGui frame in `ui::draw_over_game()`, the
same frame that draws the F12 screenshot notification. They do not replace
the toast or network/status callbacks. `GameProfile::game_overlay_visible`
keeps this frame alive even after the normal hint/toast expires, and
`game_overlay_draw` runs inside it. Ordinary overlays are hidden while the menu or
game system dialogs are open. Providers can opt into the separate persistent
path (`persistent_overlay_visible`/`persistent_overlay_draw`) to remain visible
over menus and dialogs. They never request input focus.

The renderer's screenshot behavior remains unchanged: this hook does not
promise to include ImGui mod overlays in F12 captures.

## Native providers and ZIPs

`host/mod_overlay.hpp` registers compiled-in implementations by mod ID. Register
providers during profile initialization, before the launcher first inspects
mods. Registration requires enable, disable, visibility and drawing callbacks;
duplicates are refused. Enable receives the installed mod folder for reading
data/settings. Provider callbacks run on the UI thread. The draw callback may
use ImGui; the others must not require an active ImGui frame.

An `overlay` manifest is accepted only when its ID matches a registered
provider, its game/API fields validate, and all required capabilities exist.
ZIPs supply configuration/assets; DLLs, scripts and CWCheat commands are not
loaded or executed. A provider exposes `overlay_draw` automatically and may
declare additional implemented capabilities such as `damage_events`.

The launcher persists the ordinary enabled list, restores providers on the
first gameplay UI frame, applies toggles, and disables before removal. Updated
enabled packages are refreshed on the UI thread after installation completes.
Registering a provider does not enable it by default.

The `gathering-nodes` provider exposes `gathering_nodes` and `overlay_draw`.
It reuses this same frame for head-anchored gathering icons; guest-thread capture
and tool actions are described in the Gathering Nodes guide in [Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods).

The `monster-hp` provider exposes `monster_hp` and `overlay_draw`. It enumerates the live 20-slot USA monster registry from
HUD/camera hooks, independently of damage. Quest-state checks and dedicated
reward/save transition hooks clear the bars when hunting ends. It filters named large monsters to the hunter's current area and
stacks bars upwards from the bottom-left, including over menus,
and does not depend on PPSSPP's debugger; see the Monster HP guide in [Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods).

The built-in `damage-numbers` provider now implements an impact-position renderer
and declares `overlay_draw` and `damage_events`. Numbers are bold with a white outline,
small initial pop, upward movement and a 0.9-second fade, with at most 20
animations at once. Each number retains its captured world contact point and
is re-projected as the camera moves, with a small screen-space offset above
the impact. Offscreen/behind-camera points and unavailable camera data are
hidden rather than pinned to the screen. Normal hits are light orange
(`255,190,120`), positive criticals have the same color and a smaller raised
`*`, and negative-affinity hits are red (`255,82,82`) without a star.
The installable ZIP is published separately; see the Damage Numbers guide in
[Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods).
An overlay manifest with ID `damage-numbers` can select this native provider;
normal launches leave it disabled until a matching installed mod is enabled.

## Damage capture

`host/damage_events.hpp` supplies independently enabled, read-only event
capture. Build-copy instrumentation captures the contact at entry to USA
`game_sub` routine `0x09C43558`, then publishes each accepted hit at
`0x09C44D30`, before the damage is added to the target's pending damage array.
The entry's sixth argument is also passed to the game's hit-effect routines;
it is copied immediately, before the callee reuses its register for damage.
The accepted amount is `s1`; the target is `s3` and attack record is `s5` at
commit. Contacts are matched by guest stack, target, attack and reset generation.
All cheats can remain off. The original generated corpus is never edited.

Capture checks instruction signatures, actor alignment, guest RAM ranges,
player exclusion and finite contact coordinates. Positioned events include
the accepted per-hit amount (including overkill), copied world contact,
monotonic sequence/timestamp and affinity. Attack byte `+0x2B` carries positive
affinity in `0x80` (the game's 1.25 multiplier path) and negative affinity in
`0x40` (0.75 path). Flags are read from each hit's own attack record, not a
shared last-critical flag. Complete damage-source coverage remains unverified.

The original aggregate HP-loss feed at `0x09AC7AD4` remains available for
diagnostics and cheat-order tests. These unpositioned events are ignored by
the renderer to avoid duplicates or numbers at an invented location.

## Camera and presentation

FU's ordinary GE view matrix is identity because the game pre-applies its
camera on the CPU. The native hook reads VFPU `M100` at the camera-update
return `0x0886941C`. The original archive copied this into `M700`; the native
implementation reads the original matrix directly. Its affine rows are
validated, copied and timestamped. Old camera snapshots expire after 0.25 s.

The renderer combines this matrix with the displayed framebuffer's busiest
perspective projection and GE viewport. Screen mapping follows the game's
presentation rectangle, including Original-aspect letterboxing. Projection
collection runs only while the provider is active. Render code never reads
guest actor pointers. Current validation uses original 30 FPS gameplay;
alignment with interpolated camera frames has not been verified.

A bounded, mutex-protected queue retains the most recent 64 events. UI code
drains copies; it must never dereference the actor address from an event.
Disabling clears the queue and game initialization clears old events. A reset
generation also invalidates renderer animations. The renderer discards events
older than its lifetime, so a paused dialog cannot replay a backlog. Providers
must also reset their own animations and clear capture on area/quest/session
transitions. The feed starts disabled and must be explicitly enabled by a
provider; it does not share cheat preferences.

## Remaining damage-number authoring work

Run a real combat playtest to verify individual contact positions, damage
values and affinity variants across weapons/damage sources; check interpolation
and transitions during active animations. The packaged manifest identifies
`DuduX42` as its author. Normal launches still require that mod to
be installed and enabled; preview flags are never added to the desktop shortcut.

## Verification

Version 0.2.0 rebuilt the native application and gameplay DLL; all twelve
selected overlay, cheat and instrumentation tests pass. Labeled sample
captures in `build/windows/overlay-fixes-live/` confirm the updated damage
style, head marker and named bottom-left bars. `persistent-menu-fixed.bmp`
confirms HP bars remain visible while the game is paused in the menu.
Gathering depletion/type and HP area/species filtering have guest-memory
regression tests; these visual fixtures are not real combat/harvest evidence.

Windows native application and the supported USA `game_sub` overlay built
successfully. Eight CTest targets passed, covering projection, animation,
contact capture and reset, per-hit affinity, provider lifecycle, existing
cheats and instrumentation guards. The Python instrumentation tests contain
14 cases. Saved framework changes pass reverse-apply checking.

The labeled world fixture was visually inspected in the village and Snowy
Mountains quest camp at 960x544, including captures before/after camera turns.
It uses a copied hunter position plus a fixed height to verify camera mapping;
it is not evidence of real weapon impacts. The final bounded script exited
at frame 4550 with `Runtime stopped: window closed`. Development evidence is
under `build/windows/damage-position-final/`. The run used unthrottled pacing
to shorten the check; it is not a performance measurement.

The separate normal/critical/negative-affinity style preview was inspected
under `build/windows/damage-affinity-preview/` and closed at frame 90.
`MHFU_DAMAGE_NUMBERS_PREVIEW=1` selects labeled screen samples;
`MHFU_DAMAGE_WORLD_PREVIEW=1` selects the labeled world fixture;
`MHFU_DAMAGE_TRACE=1` records diagnostic matrices and accepted hits.
No real combat impact or critical-hit playtest was performed in this pass.

The existing desktop shortcut now targets the rebuilt
`build/windows/bin/FreedomUniteNative.exe`, preserving its previous arguments
and normal user-data selection. Its original link is backed up under
`build/Freedom Unbound.before-damage-renderer.lnk`.
