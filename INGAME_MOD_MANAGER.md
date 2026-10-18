# In-game Mod Manager

Press **F9** in the game window to open or close the manager. Esc, the Close
button and the controller's Back button also close it. Mouse, keyboard and
controller navigation use the launcher's existing UI controls.

The manager lists installed mods, versions, descriptions, enabled states and
compatibility messages. Supported overlay, camera and quality-of-life mods can
be switched on or off immediately; the choice is saved for future launches.
Refresh rereads the installed list without restarting the game.

Texture packs, language packs and mods marked as changing saved progress are
listed but cannot be changed in this panel. They say to change them from the
launcher and restart. This preserves texture/font loading and startup save
backups. Installation, removal and updates remain on the launcher's Mods page.
Freedom Unbound HD has its own **F10** shortcut during gameplay to switch the
installed texture pack off/on; this does not add an option to the F9 panel.

The game keeps running while the manager is open, including in multiplayer.
Keyboard, mouse and controller gameplay input is blocked while navigating the
panel. The manager yields to save/message dialogs and the on-screen keyboard;
it does not open over another launcher menu or system dialog. F9 is reserved
and removed from older gameplay bindings, preserving alternate bindings.

Gathering Map provides the first per-mod **Options** section: independent
mining, bug-catching, regular gathering and carving checkboxes, saved locally
and applied immediately. Further mod settings will be designed with the owner.
The manager is
built into the launcher and does not need a separate mod ZIP. Existing mod
names and packages are unchanged; [official names](MOD_OFFICIAL_NAMES.md)
roll out when each mod next receives an update.

## Implementation

The framework's profile supplies an optional `game_mods_frame` callback.
The existing ImGui layer handles F9, focus and gameplay input capture; the
port draws the installed list and calls its existing mod lifecycle/persistence
API. The panel draws once per game frame and never enters a blocking menu
loop, so emulated time, audio and ad hoc networking continue. Installed files
are scanned on opening, after a change or when Refresh is pressed, rather
than every frame. Per-mod settings can extend this panel without separate
window/event loops.

## Validation — 17 October 2026

The Windows native executable and backend-independent host check built. Six
selected input, controller-prompt, mod-catalog, provider and camera checks passed,
including migration of older F9 gameplay bindings.

A bounded native quest run used isolated copied save data and eleven installed
gameplay packages. Inspected screenshots verified the list, restart-only status,
live camera checkbox and closing with F9, Esc and virtual-controller Back.
Four openings and four closings completed. Holding movement/camera sticks while
open produced no camera requests; after closing, 45 camera updates resumed.
Camera disable/enable changed the persisted enabled list and restored the initial
set of enabled mods. Performance logs confirmed guest time continued while open.
The copied save remained unchanged and the script closed its own game process.
Evidence is local under `build/windows/ingame-mod-manager-live/`.

Physical controller hardware and an active multiplayer session were not exercised
in this check. The panel uses the existing controller-navigation backend and
never invokes the pause loop.
