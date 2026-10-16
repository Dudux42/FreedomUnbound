# First alpha release preparation

## Dedicated launcher update: v0.3.0-alpha.2

Alpha.1 remains preserved as the initial private prerelease. Alpha.2 adds
`FreedomUnbound.exe` as the primary package entry point, keeping the existing
branded interface and session logging. It starts the bundled PowerShell helper
without a console window and reports failures with a Windows error dialog.
The `.cmd` shortcut remains a fallback. Gameplay logic and native FPS settings
are unchanged; native research hooks remain excluded.

This update requires a fresh standard build and regression pass, dependency and
manifest audits, ZIP verification, and an extracted `.exe` startup test with
copied saves. The executable must also reject an incomplete extraction. Both
the README and tester guide use the new entry point. The second-PC playtest
remains pending before distribution broadens beyond invited testers.

Native 60 FPS research is deferred. The candidate uses original 30 FPS simulation
with optional renderer interpolation. Mod implementation remains paused.

## Candidate gates

- Build without `-Native60Experiment` or `-ActionTimingDiagnostics`.
- Run the regression suite, then package with `-AlphaCandidate`. Packaging rejects
  builds containing either experimental option.
- Audit candidate file hashes, dependencies, overlay coverage and source freshness.
- Check extracted-package launcher startup and first-run setup on isolated data.
- Verify the supported ISO installs from the package without a source checkout.
- Perform the owner's final 30 FPS gameplay/save/reload pass on this exact candidate.
- Obtain at least one other-PC installation and gameplay result before widening
  distribution. Record platform limitations and known issues in release notes.
- Complete dependency license notices and the final downloadable archive before
  publication; the final candidate now includes the compiled and bundled
  dependency notices and a verified ZIP.

The first-run installer and existing save backups are part of the runtime; never
bundle the developer's installation, ISO, saves, settings or research launchers.
Preserved packages and prototype baselines remain untouched.

## Packaging

After Build and Test:

```powershell
./scripts/build-windows.ps1 -Step Package -AlphaCandidate
```

This produces a timestamped `FreedomUnbound-AlphaCandidate-*` folder with the
dedicated launcher executable and fallback shortcut, automatic session logs, setup/playtest instructions, libraries,
build provenance and file hashes. It is a local candidate, not a published release
or a promise that all release gates have passed. Native research packages stay
separate. A final version/tag and GitHub publication follow candidate validation.

## October 11 candidate evidence

Prepared local `FreedomUnbound-AlphaCandidate-20261011-002018` with both native
research hooks and action timing diagnostics disabled. All 25 regression targets
passed in 26.53 seconds. The audit verified all 312 recorded package files with
no invalid hashes, missing imported dependencies, PE parsing errors, stale source
records or forbidden tracked files.

The packaged executable installed the supported local ISO into an empty isolated
data directory using the command-line installer (`--in-place`), exiting 0. Bounded
launcher and game boot checks on that installation exited normally (0 and 4).
Inspected captures show the branded launcher with three empty hunter slots and
the language-selection screen at approximately 30 FPS/full speed. Testing used
20% audio volume. No original save or installation was changed.

This verifies package installation and startup on the development PC. It does
not verify the graphical ISO-selection workflow, the distributed logging shortcut
end to end, another PC, gameplay on this exact candidate or interpolation across
all scenes. Those checks and the remaining release gates are still open. The
candidate has not been published.

## October 11: automated release checks completed

The candidate for the owner's next test is
`FreedomUnbound-AlphaCandidate-20261011-003213`. Both experimental options remain
compiled out. The standard game executable and overlay hashes match those used
in the earlier successful installation/launcher checks; subsequent changes were
packaging and launch-script corrections. The existing 25 passing regression
targets therefore still apply to the game binaries.

Additional checks completed:

- Graphical first-run setup accepts the supported ISO through drag-and-drop,
  validates it, installs in place, and displays the completion screen.
- An invalid ISO is rejected with an explanatory error and no installed ELF.
- Settings opens; fullscreen returns from 1920x1080 to the original 960x544;
  Settings and Mods return to Home. Captures were inspected.
- Home displays the copied hunter and empty second/third slots. Saves correctly
  disables import before boot. Export and backup complete on copied saves.
  Backup matches all four original save files byte for byte, and the test source
  saves remain unchanged. The export test's output was moved into local evidence.
- The Windows PowerShell logging shortcut initially failed at `Get-FileHash`
  in this shell environment. It now uses .NET SHA-256 directly. Retesting verifies
  normal exit 0, stdout/stderr logs, executable identity in `session.json`, and
  unchanged copied saves. Inherited diagnostic overrides are cleared.
- The final audit verifies all 320 manifest entries, no stale source metadata,
  missing imported dependencies or invalid hashes. Notices now include
  PortableKit, Dear ImGui, tiny-AES, xxHash, the three stb headers, SDL, Vulkan
  loader and bundled FFmpeg. No fallback font is bundled.
- The final ZIP passes CRC checks; every extracted manifest entry matches its
  size and SHA-256. The logging launcher also passes from that extracted package
  in a folder with spaces. The ZIP is 108,135,821 bytes, SHA-256
  `6ee8f186812e201e98a636267057f9edf0d09ded9e36774c151a63360e3aadfd`.

Evidence is under ignored `build/windows/investigation/alpha-*-checks`,
`alpha-ready-audit.json`, and `alpha-ready-result.json`. Automated runs used
isolated data and bounded watchdogs, with 20% volume. No original save was changed
and no game was left running.

`build/windows/Alpha-Playtest.cmd` is the local owner-test shortcut. It points to
this exact candidate, uses a copied hunter in `alpha-owner-playtest/game`, starts
at 30 FPS and 20% volume, and saves logs under `alpha-owner-playtest/logs`.
Saving during this test updates only that isolated copy. The original developer
launcher, preserved packages and prototype are unchanged.

Still required: the owner's gameplay/save/reload pass, gameplay interpolation
coverage and another-PC setup/driver/controller testing. The previous house
background distortion under interpolation remains a known visual issue; 30 FPS
is the reference setting. File-browser navigation itself was not exhaustively
tested by the ISO drop test. No GitHub release has been published.

## October 11: owner acceptance run and interpolation issue

The owner completed the checklist on candidate `20261011-003213` at 30 FPS,
including additional pickaxe purchase and mining. Quest completion, village
return, house saving, quit and subsequent reload worked. During the second run,
60 FPS interpolation was selected; the saved progress loaded as expected and
another quest/save/village-return sequence completed. Mining was not repeated
in the interpolation run because the owner had not bought a pickaxe.

The two recorded sessions lasted 794.67 and 435.59 seconds, both exiting 4 with
`Runtime stopped: window closed` and the expected candidate executable hash.
Their logs contain successful encrypted save writes; the second contains
successful decrypted loads. This supports the owner's report, without claiming
full campaign, all weapons, or other-PC validation. At 30 FPS, 619 of 648 sampled
performance lines show 28–32 displayed FPS; 20 report below 95% game speed.
Scene transitions were not separately annotated, so these counts do not prove
an absence of hitches. In the interpolation run, 316 of 377 sampled lines show
more than 55 displayed FPS; six report below 95% game speed.

On returning to the house, the owner found the setting at `60 (running at 30)`.
Selecting 30 then 60 restored interpolation and reproduced the house background
distortion. The log records automatic rate changes `60 -> 45 -> 60`, then later
`60 -> 45 -> 30`; downward reasons are `presents were late`, not a deliberate
house-specific rendering guard. The menu reports the RateGovernor's current
rate. Changing the requested setting resets its ladder, explaining reactivation;
the precise scene cause of late presentations and the distortion remain open.

The base 30 FPS owner acceptance gate is passed for the tested loop, with mining
added to coverage. Interpolation is not declared visually correct. Before public
release, either validate a targeted house interpolation correction or explicitly
retain it as an experimental option with this known limitation and a 30 FPS
fallback. Another-PC installation/controller/driver coverage is still required.
Cheat-menu implementation is scheduled after the first alpha at the owner's
request. The tested candidate, archive and saves were not modified by this log
review.

## October 11: consumable coverage and release wording

The owner additionally confirms working potions, first-aid medicine, hot drinks,
rations and whetstones. Basic item-use coverage is now verified alongside mining.
This supports the first-alpha item-use gate; it does not establish that every
untested item or item-specific effect is correct. Broader coverage remains part
of external playtesting.

The owner plans to install and test the candidate on a laptop tomorrow. That
second-PC installation, gameplay, controller and driver result is still pending.

Release wording will explicitly identify 60 FPS interpolation as experimental,
describe the house-background glitch and 30 FPS fallback, and state that an
interpolation fix is planned for an upcoming update without a confirmed date.
Native 60 FPS will be listed as a separate development goal with existing
research, currently deferred during alpha preparation and excluded from the
candidate. Cheats are scheduled after the first alpha; mods remain paused.
README and the source playtest guide now carry this wording. Existing tested
candidate folders and ZIPs remain frozen; their bundled guide must be refreshed
from this source when assembling the publication package.

## After the alpha

Prioritize reproducible crashes, save reliability, controls, audio/performance and
launcher setup problems found by testers. Expand quest, weapon and map coverage.
Resume native 60 FPS and optional mods only when the owner requests them.
