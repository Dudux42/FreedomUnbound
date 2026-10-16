# Current project diagnostics

9 October 2026. Local Windows x64 audit on the existing native-60 research branch.
The working prototype and known-good packaged 30 FPS build remain preserved.
The current executable is an observation build at 30 FPS, with no bundled gameplay changes enabled. Texture-pack management is now available on the Mods page.
The separately assembled diagnostic package is
`build/windows/packages/FreedomUnite-20261009-190146/`.

## Findings and completed fixes

| Area | Result |
| --- | --- |
| Imported libraries | 304 PE binaries checked: game executable, five supporting DLLs, and 298 overlays. No unresolved file-presence dependencies or PE parsing errors on this machine. |
| Executable identity | Installed ELF matches the supported USA profile hash; compiled executable matches build metadata. |
| Preserved package | All 309 recorded files in `FreedomUnite-20261009-083626` match size and SHA-256. |
| Git exclusions | No tracked root game executable, generated corpus, overlay binary, save, or disc image detected. |
| Regression suite | All 21 tests pass after isolating inherited playtest environment switches. |
| Fullscreen restoration | Fixed explicit restoration of window size/position and preservation of maximized intent. Actual launcher-menu captures/logs verify 960×544 → 1920×1080 → 960×544. |
| Build/package provenance | Hash all local host/workflow sources; record action diagnostics and native frame rate. Packaging now rejects source changes since Build; negative check verified. |
| Package instructions | Removed obsolete claim that interpreter fallback is required. Document native 30 FPS and verification limits. |
| Launcher automation | Home/Settings navigation verified using the existing `MHFU_HOME_SCREEN=1` option. Scripted runs otherwise intentionally skip Home. |

The fullscreen implementation caches normal window geometry before entry and
completes the fullscreen transition before requesting the saved window size.
This follows the [SDL fullscreen contract](https://wiki.libsdl.org/SDL3/SDL_SetWindowFullscreen)
and [window-size contract](https://wiki.libsdl.org/SDL3/SDL_SetWindowSize).
It is generic framework behavior, with the saved framework patch updated.
Multiple-display, maximized-start, high-DPI, and repeated in-hunt transitions
still need broader coverage.

The first audit test run had one failure because its shell inherited
`PSPRECOMP_NO_INTERPRETER=1` from an automated game run. The runtime test
deliberately exercises interpreter fallback. The corrected workflow clears
playtest switches only for the test stage and restores them afterward; the
full rerun passes. This was a diagnostics isolation defect, not a gameplay crash.

The first scripted Home check instead reached language selection and auto-load,
because scripts skip Home by design. Enabling the existing Home override produced
the inspected Settings screen. Captures and raw logs are local ignored evidence.

## Native 60 FPS readiness

The prior manual village/hunt trace confirms the native 30 FPS gate. Focused
action observations are compiled and available for the next hunt test. Their
three inserted observation calls preserve all original guest instruction text.
No frame-gate or simulation-speed change has been enabled.

The remaining dependency is game-state evidence: establish which counters and
floating values control attacks, movement, animation, gathering, and quest time.
Successful startup and regression tests cannot substitute for those measurements.
See [NATIVE_60FPS.md](NATIVE_60FPS.md).

## Mod readiness and remaining coding

The source has texture replacement/dumping, camera input accumulation and driver
hooks, and device-aware host prompts. Missing pieces include launcher mod
selection, independent feature settings, combined HD/HUD pack selection,
FU camera/actor/interaction mapping, and gameplay hooks compiled into native code.

The modern camera requirement now includes both right-stick movement and
press-L facing a selected large monster. Targeting must preserve item controls,
handle target disappearance, and avoid menus/cinematics. No camera mod was
activated using unverified memory addresses.

[MOD_IMPLEMENTATION_PLAN.md](MOD_IMPLEMENTATION_PLAN.md) specifies proposed
modules, feature ownership, compatibility checks, pack composition, each QoL
dependency, target selection, and acceptance tests. It is a coding plan rather
than a claim that the planned bundles are playable.

## Texture management foundation follow-up

The launcher now exposes Mods with persistent texture replacement on/off,
checked import, and original-artwork fallback. The new composition tool combines
compatible flat PNG HD/HUD packs without modifying originals, with deterministic
priority, deduplication, and explicit rejection of unsupported formats.
Six synthetic-art cases pass; the complete suite now has 22 passing targets.
The final native-only boot check closed normally after 11.4 seconds; its inspected
language-screen capture shows about 30 FPS. The preserved package and the new
309-file package both pass hash verification, and the final import/source audit
finds no missing dependencies or stale source metadata.
Inspected native captures confirm the Mods toggle persists and the importer
accepts the two-key synthetic composed pack. No actual game artwork is bundled.
Use `build/windows/Launcher.ps1` to inspect the page before choosing Play.
See [TEXTURE_PACK_WORKFLOW.md](TEXTURE_PACK_WORKFLOW.md).

## Next playtest when the owner returns

The Modern Minimal launcher is now implemented with an embedded original
mountain-lake background, green accents and neutral dark surfaces. The approved
Freedom Unbound mountain logo is embedded into the executable and replaces the
large home-screen text title. Native window
captures verify the branded Home and Settings pages. The branded build passes
all 22 regression targets; its package and the previous green-theme package each
pass all 309 file hashes. The README logo was published through a two-file pull
request containing the README and approved PNG only. No game files or pending
runtime changes were included in that publication. Native window
captures verify Home, Settings, Mods, and the explicit Mods Back action, which
restores focus to Mods on Home. The existing fullscreen round trip was repeated
successfully at 960×544 → 1920×1080 → 960×544. Selecting Play boots to the language
screen at about 30 FPS and full emulation speed. Scripted window close reports
`Runtime stopped: window closed` (exit 4), matching the existing boot check;
launcher-only checks exit 0. A fresh hunt has not been manually tested on this
build. Further mod development remains paused. Artwork and its generation prompt
are documented in [assets/launcher/README.md](assets/launcher/README.md).

Launcher evidence lives beside the earlier audit: `modern-launcher-captures/`,
`modern-back-captures/`, `modern-fullscreen-captures/`, `modern-play-captures/`,
`modern-launcher-final-build.log`, `modern-launcher-tests.log`, and
`modern-launcher-package.log`.

Use the existing `build/windows/Playtest-Native.ps1` entry point. It starts the
latest diagnostic executable with timestamped logs and the FPS overlay.

1. Load the Test hunter and enter a familiar hunt. Stand still for ten seconds,
   perform several complete attacks, then gather several times.
2. Check normal camera/input/audio behavior and try fullscreen → window once
   during gameplay. The automated launcher round trip has already passed.
3. Finish and return to the village. If convenient, save, close, and reload as a
   regression check. A save is not required to capture action measurements.

The current test is still 30 FPS. Native 60 FPS, modern camera/L targeting,
HD/HUD bundles, and QoL options are not yet enabled. They require the mapping and
implementation gates above. Further campaign/maps/weapons, multiplayer, and
long-session coverage remain open.

## Local evidence

Under `build/windows/investigation/native-60fps/`: `project-audit.json`,
`audit-build.log`, `audit-final-build.log`, `audit-tests-clean.log`,
`audit-fullscreen-out.log`, inspected `audit-fullscreen-captures/`,
`action-copy-verification.json`, and `audit-package.log`.
Preserve this evidence locally; it includes game-derived outputs and personal
installation paths that do not belong in the source repository.
