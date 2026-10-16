# Native 60 FPS: diagnostic comparisons

**October 10: gameplay validation failed.** The player reported fast climbing,
jumping/falling, gathering/carving and HUD effects, plus visible swings that
miss contact or damage. New research packages remain diagnostic candidates;
the limited clock and startup checks do not establish gameplay correctness.

This is an experimental comparison build. The standard launcher remains on
the normal build. Each research session starts with a fresh copy of your save;
progress and saves made during that session stay in its separate folder.

## Start

The latest action-routing diagnostics need a short targeted comparison before
any flight correction can be validated. Use the same hunter, quest and ledge
in `Native30-Reference.cmd` and `Native60-Research.cmd`. In each copy, climb
once, jump or drop from that ledge once, and gather at one gathering point.
Carve once if a corpse is readily available; a complete hunt is unnecessary.
Close the game normally and report which actions still feel fast. These
launchers collect the action-state and movement traces automatically at
20 percent volume. This is a diagnostic run, not acceptance of native 60 FPS.

Double-click `Native60-Research.cmd` in the project folder. Load your usual
hunter and enter a familiar quest with a reachable ledge and gathering point.
Audio starts at 20 percent. Keep the renderer's frame-rate setting at 30;
the script enables native game updates separately, without interpolation.
The overlay should show GAME near 60 and SPD near 100 percent.
The copied session starts at 1x internal resolution. After checking action
timing, try 4x separately to check performance.

If you need a direct reference, close the research session and launch
`Native30-Reference.cmd`. It uses the same build with native timing hooks off
and another fresh copy of the original save. Use the same weapon and quest.

## Broader gameplay checks (after the targeted trace)

- Walk, sprint, dodge, draw and sheathe your weapon. Compare speed and distance.
- Use basic attacks and a familiar combo, both in the air and against a monster.
  Check attack speed, recovery, hit reactions, damage and the brief freeze on impact.
- Watch monster attacks and movement. Check whether dodging and collision feel normal.
- Gather and carve. Watch animation speed, interaction timing and any overlapping messages.
- Complete the quest, watch the return countdown, then return to the village.
- Enter the house, save in this copied session, and check menus, camera and audio.

Report the weapon, quest and area, the action affected, and whether it feels
faster, slower, or otherwise different. Mention frame drops, sound gaps,
unexpected damage or missed hits. Closing the game preserves diagnostic logs
under `build/windows/investigation/native-60fps/manual-research-*`; the script
prints the exact session folder.

## Scope

Automatic checks cover the measured animation and movement paths, menu fade
and held-button repeat, short quest-clock observations, and introductory
announcement hold, slide, cleanup and pause/resume. Interrupted announcement
holds have regression coverage for independent cleanup clocks. Actual queued
announcements, combat, monster behavior, hit-stop, quest completion, cutscenes
and broader gameplay timing still need comparison. Passing this test does not
make native 60 FPS a supported default.
