# Freedom Unbound alpha playtest

This candidate targets Windows x64 and the USA Monster Hunter Freedom Unite
release (ULUS10391). Supply your own disc image; game assets and saves are not
included. Extract the entire package to a writable folder and keep the executable,
DLLs, overlays and startup script together. Double-click **`FreedomUnbound.exe`**, then follow the
first-run setup to select your ISO. Subsequent starts open the launcher.

The dedicated executable opens the same branded launcher without a console
window and keeps session logging enabled. It uses Windows' built-in PowerShell;
you do not need to open a terminal. `Start-Freedom-Unbound.cmd` is a fallback.
Do not copy only the launcher executable or run it from inside the ZIP.

You need a Vulkan-capable graphics driver, an AVX-capable CPU and the Microsoft
Visual C++ x64 runtime. A development toolchain is not needed to play the package.

Gameplay runs at its original 30 FPS. **60 FPS interpolation is experimental**:
it smooths presentation while game logic stays at 30 FPS. A known issue can
distort the background inside the hunter's house. The renderer may also reduce
the displayed rate automatically; `60 (running at 30)` reports that fallback.
Use 30 FPS for the reference test and return to it if you see distortion.
An interpolation fix is planned for an upcoming update; no date is confirmed.

Native 60 FPS is a separate development goal. Initial research has identified
movement, reward-event and combat-validation issues; that work is deferred until
after alpha release preparation, and experimental native hooks are excluded
from this build. Planned mod bundles and the cheat menu are also deferred.
The Mods page retains existing compatible texture-pack management. Multiplayer
is unverified.

## First test

1. Extract the complete ZIP, run `FreedomUnbound.exe`, complete first-run setup,
   open Settings, then choose Play.
2. Create a test hunter, or boot the game and import a backed-up copy of your
   compatible USA save through the in-game menu's System/Saves controls. Import
   requires the game's save key and is unavailable before boot. Keep your
   original save separately and follow any restart prompt after importing.
3. Walk around the village, enter the house and check the camera, audio and input.
4. Start a familiar quest. Attack, kill and carve a small monster; gather an item;
   climb and jump from a ledge. Check timing and damage behavior.
5. Complete the quest, return to the village, save, close and reopen the game.
   Confirm hunter name, equipment, zenny and progress survived.
6. Visit several busy areas, including Jungle areas 1, 2 and 3. Check audio and
   input responsiveness alongside FPS. F3 toggles the performance display.
   F12 saves a PNG screenshot to the Screenshots folder next to the .exe.
7. Try fullscreen and return to a window. Esc or L3+R3 opens the in-game menu.
8. Optionally repeat the house and hunt checks with 60 FPS interpolation enabled.

Full campaign completion, every weapon/map/monster, extended sessions and other
PC configurations still need coverage. The owner's previously verified gameplay
loop is evidence for the project, not a substitute for testing this candidate.

The owner's current candidate test covers potions, first-aid medicine, hot drinks,
rations and whetstones, along with pickaxe mining. Other item types still need
broader playtester coverage.

## Reporting a problem

Include the package folder name, Windows version, CPU/GPU, controller, resolution,
frame-rate setting, quest/area, steps to reproduce, expected behavior and actual
behavior. Say whether it repeats. A screenshot or short video helps visual issues.

The launcher records a new session under
`%LOCALAPPDATA%\FreedomUnbound\logs`. Attach that session's `stdout.log`,
`stderr.log` and `session.json`. Logs can contain local paths; review them before
posting publicly. Do not attach your ISO or game assets. Save files are not needed
for an initial report. If startup fails before a session is created, include
`launcher-startup.log` from that same logs folder. A startup failure from the
dedicated executable displays an error dialog with the log location.
