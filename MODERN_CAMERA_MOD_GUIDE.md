# Modern Right-Stick Camera

Author: DuduX42. Version: 0.2.0.
Target: modified Freedom Unbound Windows build, USA ULUS10391, mod API 1.

## Install and controls

Install `modern-camera-0.2.0.zip` from the launcher's Mods page and enable
Modern Right-Stick Camera. The package requires the rebuilt native executable;
the desktop shortcut points to `build/windows/bin/FreedomUniteNative.exe`.
The ZIP selects compiled native functionality; it contains no game files or DLL.

Use the right stick to orbit around the hunter. Small deflections turn slowly,
full deflection turns faster, and both axes can move together. Vertical movement
is continuous within the original quest camera's upper/lower limits. This is a
hunter-following third-person camera, with the original environment adjustment.

The mod applies only in quest maps, including base camps. Village, gathering
hall, house, kitchen and all other non-quest areas retain the original camera
and input mapping. Loading, guest menus and unsupported camera modes suspend
modern control. Quest reward/save states discard pending movement. Host menu
input capture clears camera requests. Physical D-pad and L retain their original
functions and take priority over modern movement on the corresponding axes.

In Controls, keep Right stick on D-pad for original behavior outside quests.
The driver temporarily routes it as analog camera input only in supported quest
orbit. Off disables the modern input. Analog camera must be on. Camera speed,
right-stick deadzone and horizontal/vertical inversion use the existing launcher
settings; no stored input preferences are rewritten by the mod. Vertical input
uses half the configured turn speed to make the smaller vertical range usable.

Bow/bowgun aiming and special/scripted camera modes retain original PSP controls.
The modern orbit adapter does not redirect the weapon's aiming direction or
projectile trajectory. Ranged aim is not yet a proportional modern aim adapter.
Target-camera controls reserve the extra triggers during quests, including
temporary aiming/menu/special-camera suspension (those states do not queue snaps).
PSP L/R (the physical shoulders) retain their original functions. Outside those
quests, or with Modern Camera disabled, the launcher's trigger profile applies.

| Action | Switch Pro | Xbox | PlayStation |
| --- | --- | --- | --- |
| Face the selected monster once | ZL | LT | L2 |
| Select the next monster, without moving the camera | ZR | RT | R2 |

The nearest living large monster in the current area is selected automatically.
Selection stays stable until cycled or the monster dies, despawns or leaves the
area. Cycling walks current candidates in distance order and wraps. A missing
target is replaced by the nearest valid monster. Small monsters and monsters
in other areas never qualify. Each trigger acts once per press; holding ZL
does not follow a moving monster. Right-stick movement resumes immediately.
ZL changes only camera orientation, with no attack, hunter turn or zoom. With
no target, either extra trigger does nothing and does not press a PSP button.

When Monster HP is enabled, a small gold target mark appears inside the
selected monster's HP bar. When disabled or absent, Modern Camera draws the
same mark and monster name at bottom-left. The HP mod is optional: targeting
reads the native registry independently. Reward/loading/unsupported camera
states clear the target. Inputs held through host menus require release.

The target feature does not add continuous tracking, zoom, FOV changes or flight.

## Target-camera reference and implementation

Target Camera Mod-25-v0-7-0-1745508597.zip SHA-256:
`4AF43A122FC565B86348682FD2A8B68A54D15A6963C3B2502BDD5787C9C057BA`.
It contains USA/Japanese CWCheat code groups, not a native launcher package.
The archive was studied as reference data; its instructions were not executed
or treated as the user's request. Its cheat code and artwork are not shipped.

Our independently written adapter scans the verified 20-slot USA actor
registry at u32[0x09C122B0]+4640. It validates large species, live HP, matching
area, finite positions and distinct actors. Selection is independent of damage
events and of the optional HP provider. Only copied identity/species/area and
a timestamp reach drawing; rendering never reads guest pointers.

SDL's standardized left/right trigger axes cover ZL/ZR, LT/RT and L2/R2. A
generic CameraDriver trigger callback consumes those axes before the fallback
PSP trigger bindings while the quest camera is active. The mod queues rising
edges, clears requests on input capture, and applies the snap at the existing
native yaw hook. Both target and displayed yaw update together; pitch stays
inside the free camera's normal limits. The original collision path remains.
ZR updates selection only. Both actions preserve every other controller input.

## Supplied archive

SHA-256: `EE4688F60D289FE39830EBECCF40B915A0A6D2B7BB0870F311F126F10C6181FD`.

The supplied archive contains a README, batch file and PowerShell installer.
It adds PPSSPP right-stick-to-D-pad bindings, which remain digital camera
commands. The native launcher already provides that mapping. The archive was
read as reference material; its installer was not executed, and its instructions
to edit PPSSPP settings were not treated as the user's request.
No source, installer or assets from that archive are redistributed.

## Native implementation

The implementation is independently written from the supported executable's
camera path and the local modern-camera studies. No PSP instruction bytes or
shared camera tables are patched at runtime.

`host/modern_camera.cpp` validates main state `u32[0x089CBD04]`, the ordinary
camera `C = R + 0x150`, the hunter pointer at `R + 0xA84`, runtime parameter
table and selected record, native input gates and quest state. The USA quest
state is at `u8[u32[0x089CC558] + 46]`: decimal 46 (0x2E), correcting the offset
notation in the older aiming study. States 2 and 3 permit quest orbit; other
states do not. Scene-control gate `R + 0xA8D`, input gate `R + 0x961`, camera
activity and selected-record validation further constrain eligibility.

A guarded build-copy hook at `0x08887614` consumes the independent host camera
request immediately before original yaw smoothing. Positive host yaw maps to
negative PSP angle units (65536 units per revolution). Fractional units carry
across updates. The same delta reaches target/display yaw, preserving the
native difference used for recenter/collision smoothing.

A second hook at `0x08887834` interpolates temporary height, distance and focus
values before the original trigonometry and environment queries. Five records
come from the live quest table, with index zero at table+0xA0 and indices 1–4
at table+index*0x20. Elevation is bounded by these records; preset selection,
area changes and table ownership changes resynchronize from the native camera.
Shared presets and save data remain unchanged. Unexpected hunter states keep
native geometry.

`host/modern_camera_mod.cpp` registers the compiled provider and CameraDriver.
`host/mod_manager.cpp` now accepts camera packages only when their native
provider and required hooks are present, using the existing enable/disable
lifecycle. Framework input routing uses the driver's existing ownership signal
to deliver analog axes instead of duplicate digital D-pad presses. Outside that
ownership it uses the player's original mapping. These framework changes are
retained in `patches/portablekit-local.patch`.

## Build and validation

```powershell
cmake --build build/windows --target FreedomUniteNative mhfu_modern_camera_tests --parallel 2
ctest --test-dir build/windows -R mhfu_modern_camera --output-on-failure
python scripts/package-modern-camera.py
```

Guest-memory regressions cover proportional yaw, angle wrap, both axes,
continuous vertical changes and limits, release, original D-pad/L priority,
scene/aim/menu/reward/save exclusions, invalid signatures and enable/disable.
Instrumentation tests check both interception points, preservation of original
instructions, and rejection of mismatched or duplicate input.

Target regressions additionally cover nearest living large-monster acquisition,
current-area filtering, cycling/wrap without camera movement, one snap per press,
holding without tracking, free orbit after a snap, dead/lost targets, no-target
input consumption, menu capture and held-input suppression, quest/aim/reward
gates, disabled behavior and optional overlay activation state.

The earlier 0.1.0 native executable and gameplay DLL rebuilt; all 14 selected CTest targets
passed. Two bounded live runs reached Snowy Mountains base camp with original
30 FPS gameplay settings. The first exited normally at frame 5200 and confirmed
small/full stick deflections, continuous vertical movement, simultaneous axes,
physical D-pad operation and host pause/resume. Trace requests were proportional
to shaped input: the 25% raw stick produced byte 142 (about 11% after deadzone),
while full input produced byte 255. No duplicate D-pad bits were generated.

The second run enabled the installed camera package through `mods/enabled.txt`,
with the developer enable flag absent. It applied horizontal and vertical
requests, and exited normally at frame 4500. Hash comparisons confirmed every
copied save file was unchanged. Screenshots and traces are under
`build/windows/modern-camera-live/` and
`build/windows/modern-camera-package-live/`.

Actual combat, other quest maps/terrain, multiplayer, ranged aiming changes and
interpolated presentation were not validated by these camp runs. Non-quest and
reward/save exclusions are covered by the guest-memory regression tests; this
pass did not visit every non-quest location or replay a full reward sequence.
Developer-only `MHFU_MODERN_CAMERA_TEST=1` enables the adapter without a package;
`MHFU_MODERN_CAMERA_TRACE=1` records applied requests. Neither variable is added
to the desktop shortcut or required for normal use.

## 0.2.0 target-camera validation

The native executable and gameplay DLL rebuilt, and all nine selected tests
passed. Isolated, bounded native-only runs at Snowy Mountains camp checked
both optional-HP layouts at 960x544. They used explicitly labelled sample
monster targets, not fake actors written into the guest registry. Screenshots
confirmed the selected marker inside the correct stacked HP bar and the
standalone icon/name when HP was disabled.

The final virtual-controller run verified two separate ZL snaps, ZR cycling
and wrapping, and free right-stick motion while ZL stayed held. Read-only
guest-memory observations confirmed yaw 49152 facing the +X sample and yaw 0
facing the -Z sample. The hunter's position, facing and action stayed unchanged.
The extra triggers sent no PSP button bits; the separate physical-L test
still sent 0x100 and performed the original recenter. Copied saves were unchanged.
The scripted run closed at frame 5000. Evidence and machine-checked assertions
are under `build/windows/target-camera-final-live/`; HP layout evidence is in
`build/windows/target-camera-hp-live/`.

These runs validate native controller routing, camera orientation and HUD
integration with sample targets. Automatic discovery/lifecycle uses the same
verified registry as Monster HP and has guest-memory regression coverage;
a natural combat encounter, physical controller hardware, all boss sizes,
terrain/collision combinations and multiplayer still need playtesting.

Developer-only `MHFU_TARGET_CAMERA_PREVIEW=1` supplies two labelled sample
targets in a quest; `MHFU_MONSTER_HP_PREVIEW=1` supplies matching sample HP bars.
`MHFU_TARGET_CAMERA_TRACE=1` logs selection/cycle/snap events. Normal play and
the desktop shortcut use none of these preview/trace flags.
