# Freedom Unite recomp plan

Playable means a new hunter can be created, the village works, a hunt can be started and finished, and the save reloads, with picture, sound and controls. Online hunting is its own category and is not part of that bar.

## Current evidence and next steps (8 October 2026)

- 8 October: the expanded build's launch stalled before any output. A headless reproduction exceeded its 55-second watchdog. The framework now indexes canonical overlay filenames at startup and loads matching DLLs on demand, retaining metadata/ABI checks and an eager-loading comparison switch. Strict headless boot completes in about five seconds; rendered boot reaches language selection. All 20 tests pass, and the new separate package is build/windows/packages/FreedomUnite-20261008-220048/. A manual quest/reload check after this loader change remains outstanding.
- The subsequent manual test passed: the owner loaded the save, completed a quest, saved and returned to the village with fallback disabled. Logs confirm 14 DLLs loaded on demand, reuse of previously loaded stages and village libraries, successful saving and normal window closure. The updated save is backed up. Next coverage work is broader gameplay as progression permits; a cold reload of this latest save remains unverified.

- The owner verified loading an existing hunter, village movement, a completed hunt, saving, returning, quitting, reopening and a second hunt followed by a return to the village. The core gameplay loop is verified; this does not establish full campaign coverage.
- The working prototype is preserved locally, with verified file hashes and source/build records. See PROTOTYPE.md.
- The staged Windows build workflow is in scripts/build-windows.ps1; BUILDING.md describes its dependencies, required PortableKit patch, isolated installation and packaging.
- The interpreter investigation identified startup code embedded in .data and runtime overlay slots; boot loads demo_task.ovl and demo_sub.ovl. The slot map is now in the profile, with an off switch. See INVESTIGATION.md for evidence and limits.
- The seven embedded startup constructors are now compiled through the reproducible workflow. Strict boot passes them and next stops in demo_sub.ovl at 0x09C1B780; rendered boot still reaches language selection and all 20 tests pass.
- Both boot overlays are now compiled, included in package manifests, and installed successfully at runtime. Strict rendered boot reaches language selection with the interpreter disabled; development and packaged executables were checked. All 20 tests pass.
- The game menu and entry into the character editor pass with the interpreter disabled. edit_task.ovl is compiled and packaged; switching demo_task → edit_task → demo_task was observed in the selection/cancel check.
- An isolated new hunter was created and saved with the interpreter disabled, then reloaded after a cold restart. cont_task.ovl is compiled and packaged. The opening cinematic starts natively; a bounded diagnostic run of the four-library package then stopped at 0x09B0FE00 in lobby_task.ovl. Its image is preserved locally for compilation.
- lobby_task.ovl is now compiled and packaged. The strict runtime installs it and passes the previous stop, then stops at 0x09CFB680 in game_sub.ovl. That image is preserved locally; village rendering and movement have not yet been reached in strict mode.
- game_sub.ovl and the two small stage libraries stage066/stage139 are compiled and packaged. Bounded strict runs completed the opening dialogue, reached player control in the home, verified movement to the door and exited into rendered Pokke Village. These checks used diagnostic fast-forward settings, so normal-speed coverage remains to be established.
- In-game bed saving, cold reload directly into the village, village movement, the item menu and NPC dialogue now pass with fallback disabled. A normal-paced reload reached the rendered village too.
- Manual strict testing captured arcade_task.ovl, which is now compiled and packaged. A repeat run installed it and returned to lobby_task, then stopped at 0x09C10E00 in game_task.ovl on quest entry. A fallback-enabled manual run traversed eight captured stage images, returned to the village, saved and quit normally. The log establishes these transitions; it does not identify the quest result by itself.
- game_task and all eight captured stages are now compiled and packaged, bringing the library count to 18. All 20 tests and bounded native-only headless boot checks of the development and packaged builds pass. The prototype and the latest manual test save remain hash-verified and unchanged.
- The owner's next strict manual run bought and equipped armor and a weapon, entered Slay Giaprey and killed several enemies. The log records native game_task and eight quest stages, then stops on the missing stage094 library at 0x09D63680. That captured library is now compiled and packaged, bringing the count to 19. See INVESTIGATION.md for validation.
- The owner completed Slay Giaprey successfully after adding stage094. The strict manual log confirms its execution, return to lobby_task/stage139, two successful save writes and normal window closure, without a missing-library stop. The completed quest save is backed up separately.
- Next technical milestone: cold restart and reload the completed quest save, then broaden native coverage to another quest/map with the same local native playtest launcher. Continue collecting any remaining task, monster or stage images.
- An offline inventory now compares the executable's 299 declared overlay sections against the 19 compiled libraries: 256 stage, 17 monster, six task and one debug declaration have no matching DLL. These are declarations, not verified mandatory paths. The ISO contains no raw MWo3 signatures; investigate DATA.BIN extraction/decompression to obtain libraries without story progression. The reusable scanner is scripts/scan-overlays.py; its full local report is under build/windows/investigation/overlay-inventory/.
- Offline extraction has now recovered all 298 declared .ovl libraries from DATA.BIN, including all 279 not yet compiled. All 19 existing libraries match the extracted header/code fingerprints; only debug.elf was not recovered. The external research tools and images stay ignored and outside the runtime/package. Next implementation step: compile the remaining libraries in small batches, then test broader gameplay. See build/windows/investigation/archive-research/REPORT.md for local evidence.
- All 279 remaining images have now compiled in ten batches at two jobs, bringing the build to all 298 recovered game libraries. All DLL metadata/registration exports match their inputs, all 20 tests pass, and development/package headless boot checks accept 298 corpora with zero logging stubs. The separate final package is build/windows/packages/FreedomUnite-20261007-044353/. Previous package, prototype and completed-quest save hashes remain unchanged. Next: repeat the known quest/save/reload loop in the expanded build, then broaden gameplay as progression permits. Compilation covers declarations, not verified execution of every library.
- Follow with reported gameplay/usability fixes and broader weapon, map, quest and long-session coverage. Multiplayer and mods retain the scope decisions below.

The approach is the one PortableKit documents for a new PSP game, which is the framework taken out of Yakumo. Yakumo itself is a playable port of Monster Hunter Portable 3rd HD and has not been switched onto PortableKit yet. This port is a profile on PortableKit, not a fork of Yakumo. The game stays out of the repository. The copy to build against is the USA ISO already on this machine (volume label MHFU).

Nothing below assumes Freedom Unite has the same overlay count or archive layout as Portable 3rd. PortableKit has been run against the Japanese Portable 2nd G disc far enough that its main executable had no unimplemented imports left. That is a hint, not a result for this USA Freedom Unite disc.

## 1. Ground rules

- Keep the ISO, the decrypted executable, generated C++, overlays and saves out of git.
- Record the disc id from PARAM.SFO and the hash of the encrypted EBOOT once they are read. Do not guess them.
- Our own code only. No game assets in the repo, in commits, or in notes that get shared.
- Windows on this machine first. Other platforms wait until a hunt is playable here.

## 2. Stand up the framework

- Add PortableKit as the framework (submodule), with a profile file and a short CMakeLists, the layout in its profile guide.
- Build the host with no generated game code.
- Confirm that build runs on this Windows machine.

## 3. Identify this disc

- Read PARAM.SFO (disc id) and hash PSP_GAME/SYSDIR/EBOOT.BIN.
- Supply the decryption tag and key the installer needs, and confirm --install decrypts this ISO.
- Record load address, memory size and save-folder names the profile cannot read from the ELF.
- Find where code is loaded at runtime: extra modules, overlays, or an archive. Map that layout from this ISO. Do not copy Portable 3rd's DATA.BIN layout across.

## 4. Boot under the interpreter

- Run with no recompiled code, no window and no audio, and bound every run.
- List the imports nothing implements.
- Each time the game stops, implement the call it stopped on and run again. Rebuild only the host.
- Stop this loop when boot gets through the first screens, not when every import in the SDK exists.

## 5. Graphics

- Get the PSP graphics command lists drawing through the existing Vulkan renderer.
- Fix whatever this game submits that draws nothing or draws wrong: title, menus, character creation, village, hunter, monsters, effects.
- Treat Portable 3rd leftovers (curved surfaces, empty save dialogs) as things to check if we hit them, not as a copied bug list.

## 6. Audio and movies

- Sound effects and sequenced voices.
- Streamed music.
- Cutscene movies.
- Loading staying silent without stalling the game.

## 7. Saves and system dialogs

- Read and write the PSP save format, including a save copied from a PSP.
- Memory-stick paths for this title.
- The system save and message dialogs the game opens. These are a known gap on the Portable 3rd port.

## 8. Recompile the game

- Generate C++ from the decrypted EBOOT only after the interpreter boot gets somewhere.
- Count instructions the recompiler cannot lower before paying for the long compile.
- Recompile each runtime-loaded code piece into its own library and swap it in when the game loads it.
- Run with the interpreter switched off. A normal hunt must not depend on it. The first missed address stops the run and gets named.

## 9. Controls and the app

- Gamepad, keyboard and mouse, including the camera.
- A first-run setup that takes this ISO, checks it, and prepares the game outside the repo.
- Window, resolution and a way to quit and change settings.

## 10. The play loop

- Character creation.
- Village: shop, bed, quest desk.
- Accept a quest, load the area, fight, end the quest, return.
- Save, quit, and load that save.
- A second hunt from the loaded save, so it is not a one-shot boot.

## 11. Call it playable

- Write down what was tested and on which build.
- A finished hunt with the interpreter off, at the game's own pace.
- Crashes and deadlocks leave a trace that names the last system call or missed address.
- Only then look at load times and presentation frame rate.

## 12. Multiplayer, after that

- Ad hoc hunting with another player, the way the Portable 3rd port does, is the expected first route. Other methods may be looked at later.
- Keep local LAN play as an option alongside it.
- Not required for the playable bar above. Luis decides the method before work starts.

## 13. Mod support, after the launcher works

- Starts only once the launcher (category 9) works properly.
- Scope and approach to be agreed with Luis before any code.
