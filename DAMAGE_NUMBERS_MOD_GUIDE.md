# Damage Numbers: how this mod was made

Version: 0.2.0 (combat validation pending). Author: DuduX42.
Target: Freedom Unbound's modified native Windows build, USA ULUS10391,
mod API 1.

## Install and use

Drop `damage-numbers-0.2.1.zip` onto the launcher's Mods page, choose Install,
then switch Damage Numbers on. Launch through the existing Freedom Unbound
desktop shortcut, which points to `build/windows/bin/FreedomUniteNative.exe`.
Disable the mod on the Mods page to stop capture and clear its animations.
It does not change saves or enable cheats.

The ZIP selects a native implementation already compiled into this modified
launcher. It contains a manifest and this guide; it does not contain a DLL,
game files, or executable cheat commands. Older launchers without the
`damage-numbers` provider cannot use it. Installing this ZIP does not upgrade
the launcher binary. There are no user-adjustable settings in this version.

## 1. Study the original mod

The supplied Damage Numbers Display archive contained USA and Japanese
CWCheat text files. These were used as reference material to understand
damage display and camera handling. Their contents were treated as mod data,
not instructions to execute. The original archive is not redistributed in
this package. This native version targets only the verified USA profile.

The original code copied VFPU camera matrix M100 into M700 at the camera
update return. Our native build does not execute that original copy, so
reading M700 produced an empty matrix. We capture M100 directly instead.
See `DAMAGE_NUMBERS_STUDY.md` for the archive analysis.

## 2. Reuse the launcher's overlay frame

The launcher already opens an ImGui frame over gameplay for the F12 screenshot
notification. `GameProfile` now exposes visibility and draw callbacks through
that frame. This keeps the overlay alive when numbers are visible and hides
it during menus and system dialogs without taking input focus.

`host/mod_overlay.hpp` and `.cpp` register native providers by mod ID.
`host/mod_manager.cpp` validates the manifest, required capabilities, API and
game ID, persists enable state, and restores the provider on the UI thread.
The registered ID is `damage-numbers`; its capabilities are `overlay_draw`
and `damage_events`. Arbitrary scripts or CWCheat commands are not loaded.
F12 image captures are not guaranteed to include the ImGui numbers.

## 3. Capture each accepted hit and its contact point

`scripts/instrument-damage.py` inserts guarded calls into build copies of
translated code; the original generated corpus stays untouched. Each hook
checks its expected instruction before capture.

For the supported USA `game_sub` routine:

- At `0x09C43558`, copy the sixth argument's world contact vector immediately.
  This vector is also used by the game's impact effects.
- At `0x09C44D30`, read the accepted damage from `s1` before the game adds it
  to pending damage. At this point `s3` is the target and `s5` the attack record.
- Match entry and commit using the guest stack, target, attack and reset
  generation. Validate RAM ranges and finite coordinates and exclude the player.
- Read attack byte `+0x2B`: `0x80` means positive affinity, `0x40` negative
  affinity. The negative flag takes precedence if both are present.

`host/damage_events.cpp` publishes copied values into a bounded 64-event queue.
Render code never follows guest actor pointers. The amount is accepted per-hit
damage, including overkill. The older aggregate HP-loss feed at `0x09AC7AD4`
remains diagnostic only; its unpositioned events are ignored by this renderer
to avoid duplicate numbers.

## 4. Project the impact into the window

At camera-update return `0x0886941C`, copy and validate VFPU M100. The ordinary
GE view matrix is identity in this game because the camera is applied on the
CPU. Camera snapshots expire after 0.25 seconds.

The native Vulkan renderer collects perspective projection and viewport
candidates while the mod is active. It selects the busiest perspective for
the displayed framebuffer and combines it with the captured camera. The
screen mapping follows the actual game rectangle, including letterboxing.
The projection helper is `portablekit/host/gpu/overlay_projection.hpp`.

Each animation retains its original world contact and is projected again as
the camera moves. Numbers start slightly above that point and rise. Invalid,
offscreen or behind-camera points are hidden; no fallback screen position is
invented. Missing or stale camera data also hides them.

## 5. Draw and animate

`host/damage_number_layout.hpp` defines animation and colors;
`host/damage_numbers.cpp` connects events, projection and ImGui text:

- Normal hits: light orange, RGB `255,190,120`.
- Positive critical hits: light orange with a smaller raised `*`.
- Negative-affinity hits: red, RGB `255,82,82`, without a star.
- Bold colored glyphs with a white outline, a small initial pop, upward movement and a 0.9-second lifetime
  with fading near the end; at most 20 active numbers.

Disabling the provider clears capture and animations. Reset generations
invalidate old events, and expired events are discarded instead of replayed
after a dialog. Capture and rendering do not modify guest gameplay registers.

## 6. Build and package

Configure the native Windows project as described in `BUILDING.md`, with the
supported game corpus available locally. Build the application and the
instrumented overlay from a Visual Studio developer environment:

```powershell
cmake --build build/windows --target FreedomUniteNative overlay_ovl09C19100_game_sub_662DDC5878706505 --parallel 2
```

Framework changes are retained in `patches/portablekit-local.patch`. The root
CMake configuration generates the instrumented build copies and includes the
new host sources. A ZIP alone cannot provide these native hooks.

`scripts/package-damage-numbers.py` creates the manifest and archive using
Python's standard library. Run from the repository root:

```powershell
python scripts/package-damage-numbers.py
```

Output: `build/mods/damage-numbers-0.2.1.zip`. The archive has `mod.json` and
`DAMAGE_NUMBERS_MOD_GUIDE.md` at its root, without an enclosing folder.
The manifest requires both native capabilities, identifies ULUS10391, and
sets `author` to `DuduX42` and `affects_save` to false.

## Verification and remaining work

Version 0.2.0 adds thicker colored glyphs and an eight-direction white outline,
including the smaller critical star. Labeled normal/critical/negative-affinity
samples were inspected at 960x544 in `build/windows/overlay-fixes-live/`.
The native build and all twelve selected CTest targets passed. Impact
capture, affinity colors and animation timing are preserved.

The native application and instrumented game overlay built successfully.
Eight CTest targets passed for projection, layout, event capture, provider
lifecycle, cheats and guarded instrumentation. Tests include per-hit affinity,
copied contacts, duplicate prevention, reset behavior and camera validation.

Labeled synthetic previews verified colors and the raised star. A world
fixture above the hunter was visually checked in the village and Snowy
Mountains camp, including camera turns, at 960x544 with original 30 FPS
gameplay. These previews do not demonstrate real weapon impacts.

Real combat validation is still required: confirm contact placement and damage
values, positive and negative affinity, simultaneous hits, different weapons
and damage sources, and transitions during animations. Interpolated camera
alignment and complete damage-source coverage remain unverified.

Developer preview flags are `MHFU_DAMAGE_NUMBERS_PREVIEW=1` and
`MHFU_DAMAGE_WORLD_PREVIEW=1`; `MHFU_DAMAGE_TRACE=1` enables diagnostics.
They are not required for ordinary use and are not added to the shortcut.

## Version 0.2.1

Slimmed the white outline while preserving bold colored numbers and the critical
star. Diagonal outline offsets now use the same radius as horizontal/vertical
offsets. The native launcher rebuilt successfully. This ZIP selects the
compiled provider and requires the updated native executable.
