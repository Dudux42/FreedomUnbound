# Native 60 FPS investigation

Status: first native 60 FPS experiment FAILED gameplay-speed validation on
9 October 2026. Supported gameplay remains 30 FPS; the experiment is off
by default. The verified 30 FPS package and GitHub backup remain the reference.
Research is on `codex/native-60fps-research`.

## Confirmed findings

- The existing renderer setting above 30 FPS enables interpolation, not faster
  native game updates. FU currently inherits a two-vblank game-frame cadence.
- The USA executable registers the callback at `0x08859188` for interrupt 30.
  The callback increments `0x08A62D68`, compares it with two, and uses a busy flag
  at `0x08A62D64` before waking the game thread. The wake import is at
  `0x0890E070`, with this callback's return site `0x088591F8`.
- Bounded native boot traces observed approximately 60 callbacks and 30 wakes
  and flip imports per real second at full emulation speed. Captured language
  selection shows approximately 30 FPS. This verifies menu pacing, not a hunt.
- The subsequent 177.8-second manual session loaded the village and hunt
  overlays. Gameplay samples also show approximately 60 callbacks, 30 wakes,
  and 30 flips per second, at 100% emulation speed. The same gate therefore
  controls the observed village and hunt. Closing the window ended the session
  normally; exit code 4 here is the recorded window-close result.
- Local disassembly also identifies integer countdown subtraction/clamping at
  `0x0886DFAC` and conditional floating-point scaling by two near `0x088D5DB0`.
  Their exact gameplay roles and update cadence remain to be established.
- A static scan of executable sections in the main image and all 298 recovered
  overlay images found one direct call to the countdown routine, at
  `0x0886EAC0`. Its delay slot explicitly passes a decrement of one. This is
  evidence of update-count-based timing, not a seconds-based delta. The call
  uses an internal jump in the generated unit, so the current outer-dispatch
  observer cannot measure it. Indirect calls and dynamically selected routines
  are not excluded by this static scan. Do not assume this is the quest clock.
- Game instruction bytes are embedded in generated native code. Changing guest
  RAM instructions alone does not reliably change the compiled implementation.
  A future experimental patch must use explicit profile/native hooks or targeted
  regeneration, with code-signature checks and an opt-out.

## Observation build

`MHFU_TRACE_TIMING=1` installs an observation-only pre-dispatch hook in the FU
profile. It verifies measured USA instruction signatures before reading its
state. It never writes guest memory, registers, clocks, or scheduling decisions.
No framework changes were needed.

The once-per-second `[fu-timing]` records report real and emulated elapsed time,
vblanks, callback entries, wakes issued by the gate, flip imports, and sampled
counter/busy states. `[fu-timing-caller]` identifies the return addresses of
observed frame-buffer imports. Counts cover outer dispatch entries; they do
not establish that every logical update or nested routine was observed.

The playtest scripts enable this probe with the existing FPS overlay. Without
the environment flag it installs no observer. Profiling itself can add overhead.

### Focused action measurements

The Windows build workflow now accepts `-ActionTimingDiagnostics`, enabling
`MHFU_ACTION_TIMING_DIAGNOSTICS` in CMake. This option defaults to off. It builds
copies of generated units 0023, 0026 and 0052 under the build directory, with six
observation calls: countdown entry, its measured caller's continuation, the
floating-point routine before and after its state callbacks, and two shared
animation-advance paths. Original generated files
are never edited. Missing or ambiguous labels fail the diagnostic build.
All guest instructions in the two copies were verified text-identical after
removing the six added calls and the diagnostic header include.

With `MHFU_TRACE_TIMING=1`, the added calls sample bounded, signature-checked
guest memory. `[fu-action]` reports per-window calls and raw object fields:
integer offsets `0x10`/`0x678`, decrement argument, and (for the floating-point
routine) identifier at `0x324` and float at the secondary object's `0x14`.
These labels describe offsets, not established gameplay meanings. The count of
distinct records is capped at 32 per reporting window, with omitted/invalid
sample counts logged. Internal sampling remains observational: no guest register,
memory, clock, or scheduler writes. No action samples appear at language
selection; actual hunt coverage is still needed.

Building normally without `-ActionTimingDiagnostics` restores the original
generated units. The runtime timing flag also disables sampling when unset.
The new diagnostic build passed all 21 regression tests and a 15.4-second bounded
rendered check on copied saves. The inspected language-selection screenshot
shows approximately 30 FPS, and the process closed normally. This is a boot
check, not evidence of correct combat or of a working 60 FPS mode.

Next manual measurement: in a hunt, stand still for about ten seconds, perform
several attacks, and gather several times, allowing each action to finish.
Close the game afterward. Those distinct phases help identify which sampled
fields belong to action countdowns or animation steps. No save is required.

## Verification

- Windows Release build passed.
- All 21 regression tests passed.
- Bounded rendered checks used copied saves, exited normally, and retained
  approximately 30 FPS. A screenshot was inspected at language selection.
- The first capture failed because its output directory did not exist; creating
  it and repeating a short bounded check produced the inspected screenshot.
- No native 60 FPS gameplay, combat, movement, or timer behavior was tested.
- The manual 30 FPS village-to-hunt timing baseline passed. Later hunt samples
  report roughly 5–6 ms guest work plus 6–7 ms CPU rendering per game frame,
  with GPU work around 1.4–1.6 ms. This suggests headroom in those samples,
  but does not establish sustained 60 FPS performance or correct action timing.
  Audio reports zero dropped frames; silent-frame counts also include legitimate
  silence and cannot establish perceptual audio quality.

Local evidence is under `build/windows/investigation/native-60fps/`. Game-derived
disassembly, images, and logs remain ignored and are not part of the repository.
The gameplay baseline is `manual-native-20261009-090901/` under the same
investigation parent. `direct-timing-calls.json` records the static call scan.
Focused-build evidence includes `action-build.log`, `action-tests.log`,
`action-copy-verification.json`, and `action-boot-result.json`.

## Next stages

### Latest manual feedback (9 October, evening)

The player reported a village presentation rate initially around 45 FPS, later
holding 60 FPS before and after a quest. Combat felt normal and presentation
remained smooth at 4x internal resolution. The house background appeared to shift
incorrectly while furnishings such as the bed and item box remained stable.

This session tested renderer interpolation, not native 60 FPS. The earlier chat
instructions incorrectly described the renderer setting as a native test mode.
The session log `manual-native-20261009-224434/stdout.log` explicitly records
`fps 45.9 game 30.0 speed 100%` with `interpolation 45 of 60`, and later
`fps 60.0 game 30.0 speed 100%` with `interpolation 60`. The automatic renderer
rate governor can lower presentation to 45 and retry 60 after spare time returns;
this explains the observed change without a change in simulation cadence.
The precise initial fallback trigger has not been isolated.

The current settings also enable fast loading. Accelerated loading therefore
does not establish faster native gameplay. The house symptom is consistent with
incorrect interpolation matching or camera motion applied to background draws,
but no same-scene 30/60 capture or isolated house draw trace has confirmed this.
Compare the house at the game's own 30 FPS setting before altering rendering
logic. Preserve the working package and saved data during this investigation.

The subsequent player comparison confirmed the house renders correctly at 30
FPS. This isolates the reported house regression to interpolation. It does not
verify how the house will behave with native 60 FPS.

The smooth interpolated quest is useful renderer feedback, not validation of
native 60 FPS attacks, monster simulation, or timers.

### Main-image native timing experiment

**Manual result: rejected for gameplay use.** The player reported visibly faster
main-menu animation, save selection and village gameplay. No additional manual
30 FPS test is needed to establish this regression. The 51.6-second session
`manual-research-20261009-233123-293/` closed normally, but normal exit and 60 FPS
do not make its timing correct. In particular, `speed 100%` measures the emulated
clock against wall time, not how quickly frame-based actions progress.

The trace shows movement-state samples still reaching a `float14` step of 2.5
at roughly 60 calls per second, while the earlier 30 FPS baseline also reaches
2.5 for that state. Idle samples at 1.0 therefore did not establish movement
compensation. This narrows the investigation to additional state-dependent
rate assignments and frame-based menu/task updates; it does not yet identify
every write or prove a universal timing variable exists.

The diagnostic copies now also sample `0x088D5ED4`, after the animation-state
callbacks and immediately before the next shared update call, in addition to
`0x088D5E30` before those callbacks. Reports include raw actor float `0x69C` and
animation-object float `0x54`, alongside `0x14`. The new signature was measured
from the supported ELF. These additions only observe guest state. Compare the
before/after paths and trace the relevant overlay assignments before applying
further compensation. UI timers need separate mapping. Do not halve the virtual
clock or skip every second whole update as a substitute for native 60 FPS.

Static inspection explains one coverage gap: the branch at `0x088D5DF4`
retains an existing animation-object `0x14` rate when it exceeds the cap. Changing
the cap from 2.0 to 1.0 does not halve that retained value. Village overlay code
also writes 2.5 to actor offsets `0x94` and `0xD4` at `0x09AAB148` and
`0x09AAB14C`. Their consumers and gameplay meanings still need verification;
these are candidates, not approved patch points. The preceding constant occurs
in a branch delay slot, whose inlined generated copies must also be considered.

The expanded observation build compiled successfully. Removing its observation
calls reproduces the original generated units exactly. Two bounded checks
closed normally with copied saves unchanged: `native-comparison-20261009-233841`
reached character selection, and `native-comparison-20261009-234218` reached the
village loading screen. Neither exercised the new animation samples, so runtime
validation of the after-callback point remains pending. The standard executable
still has native research hooks compiled out.

`-Native60Experiment` in the Windows workflow builds guarded local copies of
generated units 0021, 0023 and 0026. The originals remain untouched. Runtime
activation additionally requires `MHFU_NATIVE_60_EXPERIMENT=1`; default builds
ignore this flag. This is a research foundation, not a supported gameplay mode.

The initial failed experiment changed three animation producer multiplier/cap
constants from 2.0 to 1.0. The next candidate restores those original constants
and halves the step where the shared consumer reads it instead. This includes
both advancement paths, the matching root-motion lookahead queries, and the
inlined copy of a branch delay slot. Stored rates remain unchanged; retained
rates are therefore scaled once per consumption instead of accumulating changes.
The measured frame-gate threshold remains one vblank, and the measured countdown
decrement retains per-object fractional carry. Paused countdown calls do not
consume carry. Build-time
instruction matching and startup guest signatures reject unsupported inputs.
The kernel vblank frequency, virtual clock and audio clock are unchanged.
The profile advertises the research cadence to the renderer so its 60 Hz output
does not require interpolation. Fast loading is disabled in comparisons.

Even with those corrections, overlays may contain independent movement,
physics, AI, animation and integer timers. Main UI and camera increments are
also not fully mapped. Do not describe a successful native boot as a working
native 60 FPS game. Do not use this experiment with the player's original saves.
There is no launcher toggle for this incomplete mode.

### Animation-consumer candidate

The shared actor update at `0x08865648` calls the animation advancement routine
at `0x088637C4` with the actor's animation object (`actor + 0x80`). That routine
reads a channel's `0x14` rate and adds it to `0x10`, including its boundary path.
The candidate scales reads at `0x088638C0`, `0x088638D4`, `0x08863978`,
`0x08863994`, `0x08863BE8`, and `0x08863C44`. `0x08863978` is also inlined in
the branch at `0x08863974`; both copies are changed. The last two reads belong
to the matching root-motion lookahead calculation. Instruction words were
measured independently from the supported local ELF and checked at startup.

New samples at `0x088638C8` and `0x0886399C` report the consumed floating-point
register as `effective_min/max`, together with the unchanged stored rate. This
allows verifying a stored movement rate of 2.5 is consumed as 1.25 in native
mode, and as 2.5 with the experiment off. These are timing measurements, not
proof that all movement or combat systems are compensated. Blend transitions,
hit-stop counters, independent movement and menu/task counters remain incomplete.

The candidate built successfully and passed all 25 regression targets. Its
generated consumer copy was verified to differ only at the seven intended rate
reads (six labels plus the inlined delay-slot copy). The initial bounded native
village run, `native-comparison-20261009-235104`, measured the retained 2.5 rate
consumed as 1.25. Idle's stored 2.0 rate was consumed as 1.0. It closed normally
with copied saves unchanged. A subsequent off-mode village run measured the
original 2.0 consumed unchanged at 30 FPS.

Diagnostics now separate animation channels instead of merging their call
counts. The bounded readiness-driven runs `native-comparison-20261010-000050`
(native) and `native-comparison-20261010-000336` (reference) both used the same
copied save and a two-second W press after loading. Their inspected start
captures show the same starting position. Native player state 2 consumed a
stored rate of 2.0 as 1.0 at approximately 60 calls per channel per second;
reference state 2 consumed 2.0 at approximately 30 calls. Nevertheless, the
native endpoint is farther along the route. This is visual evidence that
independent movement remains too fast, not a measurement of its exact factor.
No normal-speed claim or new user playtest is justified by this candidate.
Both checks closed normally with copied saves unchanged, in 35.1 and 53.7
seconds respectively.

An earlier live-input check reached its 55-second watchdog because CRLF input
left a carriage return on the quit command; its owned process was stopped and
copied saves retained their hashes. The harness now writes LF commands. Earlier
virtual-stick phases did not establish a movement-state comparison and are not
used as travel-speed evidence. `-VillageWalk` waits for the measured player idle
sample and the entrance transition before sending the verified keyboard path.

The partial candidate is preserved separately as
`packages/FreedomUnite-Native60Research-20261010-000429/`. It replaces producer
constant changes with consumer scaling, but remains unsuitable for a gameplay
release. The normal build is restored with the research option off after the
investigation.

The restored standard build passed all 25 tests and is preserved as
`packages/FreedomUnite-20261010-000714/`. A 24.5-second copied-save boot
(`native-comparison-20261010-000622`) deliberately requested the research mode;
this compiled-off executable still measured 30 game FPS at normal clock speed,
closed normally, and retained save hashes. Independent movement and UI work is
the next implementation step; the consumer candidate is not ready for a user
acceptance test. The final dependency/source audit found no missing libraries,
stale sources or forbidden tracked files. All five audited packages, including
the preserved smooth-Jungle version, retained all 309 manifest hashes each.

The shared main loop at `0x0888FD38` calls the frame/presentation helper, then
additional task updates and an indirect object callback at `0x0888FD80`. It
increments a separate counter at `root + 0xF7A554` at `0x0888FDF0`; the root is
read from `0x08A632C0`. Static consumers use low bits of this counter for UI
blinking, including main-image `0x08841B58` and village `0x09A81598`. This is
evidence of frame-based UI behavior, not proof that the counter controls every
menu transition. Do not simply skip the common task loop: it also updates input,
other managers and presentation. Mapping local transition/fade counters and
their consumers remains the next menu investigation.

`scripts/check-native-60.ps1` compares the off/on paths on separate copies of
the prepared diagnostic data, with bounded runs, captured screenshots and save
hash checks. The test suite checks countdown elapsed-time arithmetic and
generated-hook signature rejection/composition. Manual combat, damage, action
durations, monster behavior, movement and quest completion remain unverified.

The first comparison (`native-comparison-20261009-230821/`) measured normal-speed
30 FPS with hooks off and approximately 60 actual game frames per second with
hooks on, at language selection. Both runs closed normally and retained all
copied save hashes. A later native village load also closed normally with
unchanged saves. In the longer 44.7-second run
(`native-comparison-20261009-231640/`), settled village samples show approximately
59.8-60.1 presented FPS, 59.9 game FPS and 100% emulation speed, with around
5.3-5.6 ms guest work, 5.7-5.9 ms CPU rendering and 1.2 ms GPU work. The measured
player animation routine runs roughly 60 times per second with a step of 1.0,
versus the reference's 30 calls and step of 2.0. The captured stationary village
was inspected; this does not establish motion or action-duration correctness.

Early automatic screenshots caused extra performance dips; the longer sample
disables them and takes one requested capture. One slower reference load reached
its 55-second watchdog and was terminated, with copied saves unchanged. Other
bounded reference menu/character-selection runs completed normally. There is
not yet an identical-duration native/reference village motion comparison.
All 25 regression targets pass, including the two new timing targets.

The experiment is preserved in
`packages/FreedomUnite-Native60Research-20261009-231858/`. The standard build was
subsequently rebuilt with the native research option off, passed all 25 tests
again, and was packaged as `FreedomUnite-20261009-232256`. A bounded standard
boot with the research environment flag deliberately set still measured 30
game FPS at 100% speed, confirming that this build cannot activate the hooks.
Final dependency/source audits found no missing dependencies or stale source
files. Both packages and the preserved 22:28 and smooth-Jungle packages retained
all 309 manifest file hashes. The research-launcher preparation check retained
the original save hashes. Package paths above are relative to `build/windows/`.

For user-operated comparisons, `Native60-Research.cmd` selects the preserved
research package, checks its manifest, copies the current installation's save
and settings into a fresh ignored session directory, disables interpolation and
fast loading, and enables the research hooks. `Native30-Reference.cmd` uses the
same package with hooks off and a separate fresh copy of the same save. The
regular `Launcher.cmd` remains on the standard build. Progress in either
research session is deliberately separate from the normal save.

Do not request another manual comparison of this failed experiment. Correct
retained animation rates and map menu/task timing first. A subsequent candidate
will need movement, roll, attack, gathering and quest-clock checks against the
recorded reference. GAME near 60 and clock speed near 100% alone are insufficient
acceptance criteria. Changing the renderer frame-rate setting is not how this
native mode is activated.

1. Completed: record a normal village-to-hunt session with the timing probe and
   confirm the same gate controls gameplay. The trace counts frame pacing;
   it does not identify the duration of individual attacks or gathering actions.
2. Map simulation step values and distinguish elapsed-time updates from integer
   frame counters. Record reference action durations and quest-clock progression.
3. Introduce an explicitly experimental native mode: one-vblank pacing plus
   verified timing adjustments. Preserve the renderer's real-time pacing and
   audio clock; do not speed the whole virtual machine or enable interpolation
   to produce a misleading FPS result.
4. Compare movement, attacks, hit detection, damage, monster behavior, gathering,
   countdowns, cutscenes, audio, and saves against 30 FPS. Broaden weapon and quest
   coverage before considering 60 FPS a supported default.

## Root-motion correction: October 10

A bounded position-write trace identified the shared actor integrator at
`0x08865688` as the writer of the player translation during the village walk.
Its motion query at `0x08863B70` has a second branch that samples the previous
animation position. That branch still read the full stored rate at `0x08863CFC`,
although animation advancement was already halved. The measured USA instruction
is `0xC6200014`. The research build now halves this consumed rate too, with an
instruction signature guard. There are eight corrected reads including the
inlined branch delay slot; stored producer rates remain unchanged.

An observation at `0x08863D00` confirms the player motion query consumes 1.0
from a stored rate of 2.0 at approximately 60 calls per second. The clean copied-
save run `native-comparison-20261010-001824/native60` completed normally in
36.0 seconds, with save hashes unchanged. Its two-second keyboard walk ended
visually close to the earlier 30 FPS reference's endpoint, unlike the previous
candidate's much longer travel. This is evidence for that village movement
path, not a numerical distance measurement or proof of all gameplay timing.
A fresh hooks-off run, `native-comparison-20261010-001920/baseline`,
completed normally in 54.4 seconds with unchanged saves. Its motion-query step
was 2.0 at 30 calls per second; the inspected walk endpoint closely matches the
corrected native run. This strengthens the comparison for this specific path.
The position watch itself roughly halved host speed, so its run is used only
for identifying writers, not for the full-speed comparison.

Automated comparisons and manual research copies now set audio volume to 20%.
Normal installation settings and saves are not changed. The corrected candidate
is preserved as `FreedomUnite-Native60Research-20261010-002058`; the normal
executable was restored with research hooks compiled out and packaged separately
as `FreedomUnite-20261010-002300` (both under `build/windows/packages/`).
All 25 regression tests
passed for the corrected research build. Menus, fades, blend counters, hit-stop,
combat, monsters, gathering and quest timing still require verification or
correction before another general gameplay test is requested.

## Menu timing research: isolated consumers and fades

The shared `root + 0xF7A554` counter must not be halved globally. Its consumers
include UI blink masks, alternating work at `0x088B9128`, and state values.
The experiment instead scales only three measured bit-mask blink reads:
`0x0883B728`, `0x08841B58`, and `0x088F5FFC`. Their signatures and observer
composition are guarded. The boot/character-selection sequence did not exercise
these three points, so their visual cadence remains unverified; this is not a
claim that title or character-selection animation is fully corrected.

Character-selection overlay `cont_task` uses offset 28 as a state number, not
an elapsed timer. Its fade commands lead to shared main-image `0x0884D754`,
which stores duration at offset 36 and resets elapsed at offset 38. States 1
and 2 increment elapsed at `0x0884D348` and `0x0884D418`; rendering calculates
opacity from elapsed/duration and waits for elapsed to reach duration.

The observation-only reference `native-comparison-20261010-003116/baseline`
measured 500,490 microseconds between elapsed 0 and 15 for a 16-tick fade. The
uncorrected native run `native-comparison-20261010-003204/native60` measured
250,245 microseconds. The reference hit its short watchdog with unchanged
save hashes; the native run exited normally in 22.9 seconds. The reference
samples establish fade cadence, despite that incomplete scripted run.

The research correction uses a separate fractional carry for each fade object,
reset whenever a new fade is commanded. Both fade directions retain their
stored duration, state values, rendering and input updates. Only elapsed
increments are halved. Tests cover restarting one-tick and sixteen-tick fades,
plus exact instruction rejection and composition with the read-only observer.
The completed comparison `native-comparison-20261010-003631` measured a median
500,490 microseconds for elapsed 0 to 15 in both modes (14 intervals each).
Both runs closed normally: 48.7 seconds for the reference, 22.6 for native,
with copied save hashes unchanged and audio at 20%. These total run times
include different real-time spacing of frame-scripted input and do not measure
fade duration. Captures were inspected; they show different selection phases,
so they are not treated as matched-frame visual or animation-speed evidence.
The native capture includes a temporary loading/performance dip; this menu run
is not a performance acceptance test. The fade arithmetic comparison uses
virtual timestamps with independently paced guest time, not capture FPS.
All 25 tests passed. The candidate is preserved separately as
`build/windows/packages/FreedomUnite-Native60Research-20261010-003742`.
The standard build was restored with research hooks compiled out, passed all
25 tests again, and was packaged as `FreedomUnite-20261010-004120`. A bounded
startup with the experimental environment flag set still measured 30 game FPS
at 100% speed and retained save hashes. Both packages passed dependency and
manifest audits. Other menu animation, input-repeat timing and gameplay
counters remain open.

## Held-input repeat timing

The USA controller sampler `0x08868F50` polls `sceCtrlReadBufferPositive` and
computes held, fresh-press, release and repeat masks separately. For an unchanged
held mask, `0x08869398` increments the input object's byte at offset 20. At 13
it emits a repeat and resets the counter to 10, giving a 13-update initial wait
and a 3-update subsequent interval. Changed masks emit fresh events immediately
and clear the counter at `0x088693D4`. This counter must not be confused with
the controller sample clock, analog input, or frame-independent host bindings.

The bounded `native-comparison-20261010-004636` language-menu comparison held
Down for 120 reference frames and 240 native frames. Both runs closed normally
(26.2 and 19.3 seconds), retained copied save hashes, and used 20% volume.
Reference timestamps show 433,758 microseconds from fresh press to first
repeat, then 100,098 between repeats. Before correction, native timestamps show
216,879 and 50,049 respectively. Thus the observed input repeat runs twice fast.

The experimental correction halves only this counter increment using a carry
per input object, reset on every changed held mask. Fresh press/release masks,
analog sampling and the game's 13/10 threshold values stay unchanged. Signature
guards cover both modified instructions, and synthetic instrumentation tests
reject missing or unfamiliar instructions. Fractional-counter tests include
13-tick and 3-tick intervals. The corrected comparison `native-comparison-20261010-005254` measured identical
433,758-microsecond initial delays and 100,098-microsecond median repeat
intervals in both modes. Both produced 37 Down events over the equal-duration
hold, emitted immediate fresh Down/Up events and releases, and ended at the
same inspected language selection. The native endpoint capture measured 60
presented FPS, 59.9 game FPS and 100% clock speed. Both runs exited normally
(27.1 and 20.0 seconds), retained all copied save hashes, and used 20% volume.
All 25 regression tests passed. Reversing the two generated input edits
reproduces the observation-only copy exactly. The candidate is preserved as
`build/windows/packages/FreedomUnite-Native60Research-20261010-005444`.
The standard executable was restored with research hooks compiled out, passed
all 25 tests again, and was packaged as `FreedomUnite-20261010-005658`.
A bounded startup with the experimental flag set still ran at 30 game FPS and
100% speed, retained save hashes and closed normally. Package audits found no
missing dependencies, stale sources, invalid manifest files or forbidden
tracked game files.

Character selection's preview uses the shared actor animation consumers, for
which the earlier menu logs contain player samples with stored step 2.0 and
consumed step 1.0 in native mode. This establishes the consumed rate at those
points, not full preview-cycle duration or blend timing. No additional preview
or title-animation patch is inferred from screenshots at different phases.

## Animation blend measurements: October 10

The shared animation updater uses a separate transition countdown at channel
offset 48, a blend weight at 52, and a retained increment at 36. Setup at
`0x0886401C` stores the requested duration; the following instructions store
both the increment and initial weight as `1 / duration`. The updater adds the
increment at `0x08863894`, decrements the countdown at `0x088638A0`, and clears
the blending flag at `0x088638BC`. The immediate-change path clears that flag
at `0x088640B0`. These are independently measured USA instruction facts.

Observation-only probes cover setup, weight advancement and completion. The
reference run `native-comparison-20261010-011332/baseline` reached the village
but hit its 55-second deadline before a walking comparison; its copied save
hashes were unchanged. Its two completed three-update NPC blends had 33,366
microseconds between updates and 66,732 from setup to completion. The native
run `native-comparison-20261010-011744/native60` completed the village walk
normally in 45.2 seconds, retaining save hashes. Seven three-update blends
still took only three updates, now 16,683 microseconds apart, and completed in
33,366 microseconds. Their final weight was the same `1.33333337`. This
establishes the doubled blend speed, separately from normal animation rates.

The research correction halves the consumed weight increment and countdown
decrement together. It preserves the stored duration, increment and initial
weight. A carry belongs to each channel, resets on setup, and is released on
completion or immediate cancellation. Instruction guards cover all five
modified instructions; unfamiliar generated instructions fail the build.
Regression checks include odd durations and interrupted independent counters.
The initial-weight convention means comparison must also examine completion
phase and weight, rather than claiming correctness from update counts alone.

The corrected run `native-comparison-20261010-012835/native60` completed the
requested village walk and exited normally in 46.8 seconds, preserving copied
save hashes. Six completed three-tick blends used six updates; five-tick and
six-tick blends used ten and twelve respectively. Updates remained 16,683
microseconds apart. Final weights matched the original `1 + 1 / duration`
within floating-point rounding (for example `1.33333325` versus `1.33333337`).
Because the reference consumes its first whole tick during setup, while the
research countdown starts with a half tick, three-tick completion now occurs
83,415 microseconds after setup versus the reference's 66,732. This is a
16,683-microsecond phase difference, not exact wall-clock equivalence. Initial
blend phase remains a follow-up before combat timing acceptance. The observed
progress is no longer doubled, and the stored initial pose is unchanged.

All 25 regression tests passed. The candidate is preserved as
`FreedomUnite-Native60Research-20261010-012717`; its dependency, source-hash and
manifest audit passed. One earlier corrected run reached the village but timed
out during loading; another exited from character selection without walking.
Neither is counted as a completed walking comparison. The accepted endpoint
capture showed 52.4 presented FPS, 59.9 game FPS and 87% clock speed during
walking, so this step does not establish sustained full-speed native gameplay.

The normal executable was restored with research hooks compiled out, passed
all 25 tests, and was preserved as `FreedomUnite-20261010-013227`. Its bounded
startup with the research flag set still measured 30 game FPS at 100% speed,
exited normally in 24.7 seconds, and retained copied save hashes. Final audits
of both packages found no missing libraries, stale build sources, invalid
manifest files or forbidden tracked game files. Normal saves and prototype
baselines were not modified. Native quest timing, hit-stop and combat remain
unverified; this candidate is still for diagnosis.

The village harness now requires `lobby_task` to be installed before accepting
the idle-player sample, and rejects a normal exit if the requested walk did not
finish. Character preview samples alone are not village gameplay evidence.
Earlier movement evidence remains valid: its recorded native run also loaded
both `lobby_task` and `game_sub` before walking. Some new startup sequences
stopped at the character confirmation prompt; inspected captures identified
that input issue, and those runs are not timing acceptance results.

## Blend start-phase correction

The previous candidate started its countdown with a fractional tick, although
the measured reference consumes a whole tick on the same update as setup.
This caused a one-native-frame delay at completion. `BlendClock` now preserves
that initial whole tick, then alternates zero/one countdown decrements. Weight
advancement likewise consumes the original increment on the first update and
half the increment on subsequent updates. Stored duration, increment and
initial weight remain untouched. A duration of `N` therefore uses `2N - 1`
updates, ending at the same elapsed guest time as the reference's `N` updates.
A one-tick transition completes immediately in both modes.

The new pure timing checks compare weight and remaining duration at every
reference frame for one-, three-, five-, six- and sixteen-tick transitions,
including independent interrupted/restarted channels. The generated wrapper
still changes the same guarded USA instructions and records observations before
compensation. Runtime phase and gameplay acceptance require measured traces;
the arithmetic tests alone do not establish them.

All 25 research regression tests passed. The accepted native run
`native-comparison-20261010-014223/native60` completed a village walk and exited
normally in 46.4 seconds with copied save hashes unchanged. Seven three-tick
transitions used five updates and completed in 66,732 microseconds. Six- and
five-tick transitions used eleven and nine updates and completed in 166,830
and 133,464 microseconds. These remove the extra 16,683-microsecond phase delay
in the previous candidate.

The fresh hooks-off run `native-comparison-20261010-014355/baseline` produced
matching 66,732- and 166,830-microsecond durations for the measured three- and
six-tick transitions. Paired player channels `0x090B3600` and `0x090B3640`,
and NPC channels `0x090E40F0` and `0x090ED6A0`, also matched weight after each
reference update within `1e-6`. Probes record weight before increment, so this
comparison uses the next observation's stored weight or the completion
observation; raw pre-increment values at equal timestamps are not equivalent
to the completed-frame pose. The reference finished its two-second walk and
captured its endpoint, but the 55-second watchdog stopped it just before its
scripted quit. It is valid transition-trace evidence, not a normal-exit test.
Save hashes were unchanged. The earlier native loading timeout is likewise
not counted as a completed walking test.

Inspected endpoint captures show the village and player in both modes; the
native capture reports 60.5 presented FPS and 59.9 game FPS at 101% clock speed,
the reference 30.1 and 30.0 at 100%. These individual captures do not establish
sustained performance, quest timing or combat correctness. The candidate is
preserved as `FreedomUnite-Native60Research-20261010-014513`, whose package
manifest and dependency audit passed.

The normal executable was restored with native research hooks compiled out,
passed all 25 regression tests, and was preserved as
`FreedomUnite-20261010-014909`. A bounded copied-save startup with the research
flag set still ran at 30 game FPS and 100% clock speed, closed normally in
24.9 seconds, and retained save hashes. Final audits of both packages passed:
309 manifest files each, no missing libraries, no stale source hashes, no
invalid files, and no forbidden tracked game data. Original saves and working
prototype baselines remain untouched. Quest-clock, hit-stop and combat timing
still require their own measurements.

## Quest countdown instrumentation

The quest countdown now has a dedicated entry/return observer, independent of
the capped animation sample table. The supported USA helper is `0x0886DFAC`,
with its guarded return at `0x0886DFF8`. Its caller checks quest state 2 and a
pause byte at state offset `0x422`. It decrements two independent positive
counters at object offsets `0x10` and `0x678`, clamping each to zero. The earlier
interpretation of the second as a mirror was incorrect: the branch-likely
instruction at `0x0886DFC4` (`0x58600006`, measured from the supported ELF)
annuls its delay-slot load when the primary counter is positive. During the
measured gathering quest the primary starts at 90,000 and the secondary stays
zero. The navigation pacing guard now requires only the positive primary
counter; requiring both incorrectly kept the first quest run accelerated.

`scripts/check-quest-clock.ps1` runs for at most 55 seconds with copied game data,
20% audio volume, interpolation disabled and normal save files untouched. The
opt-in `MHFU_TIMING_NAVIGATION=1` accelerates navigation only, then restores
normal kernel pacing at the first positive active countdown. Logs record both
guest time and a fixed monotonic real-time reference, pause state, remaining
ticks and update calls. The runner reports a countdown observation only when
at least three normally paced samples from the same active object show an
actual decrease; exploration screenshots alone cannot pass this check.

The purple-clad NPC in the initial route was the retired hunter, not the Chief.
His captured dialogue explicitly identified him. The corrected route reaches
the Chief by the large ore boulder, selects one-star Mountain Herb Picking,
accepts it and uses Square at the exit to depart. It retains separate presses
for the dialogue and menus, scaled to the same reference duration in both modes.
No quest-clock compensation was changed in this step. Pause/resume, quest
completion, hit-stop and combat remain unverified.

All 25 research regression checks passed after the fixed real-time reference
was added. The diagnostic candidate is preserved as
`FreedomUnite-Native60Research-20261010-024414`. The normal executable was then
rebuilt with native research hooks compiled out, passed all 25 checks and was
preserved as `FreedomUnite-20261010-024636`. Its copied-data boot check closed
normally in 23.8 seconds, left save hashes unchanged and still ran at 30 game
FPS and 100% speed with the research flag set. The inspected capture is the
language menu; this is a boot check, not another gameplay acceptance test.
Both package audits passed with 309 manifest files each, no missing libraries,
no invalid package files, no stale build sources and no forbidden tracked game
data. Exploration runs reached dialogue but recorded zero active quest samples;
none counts as countdown timing acceptance. Normal saves and the preserved
working prototype were not modified.

## Active quest countdown comparison

The new observer and corrected navigation guard captured the same 50-minute
gathering quest in both modes. `quest-clock-20261010-035236-baseline` closed
normally in 44.2 seconds; `quest-clock-20261010-035758-native60` closed normally
in 53.1 seconds. Both retained copied save hashes and produced six active,
normally paced countdown samples. The preceding native run reached acceptance
but hit the 55-second watchdog before departure; it is not timing acceptance.
Menu waits were shortened for the successful native route, retaining adequate
time for each menu transition. Navigation remains deliberately accelerated;
those navigation FPS readings are not performance results.

Excluding the first and last samples consistently in both traces leaves four
interior observations. The primary counter decreased by 45 ticks over exactly
1,501,470 guest microseconds in both modes. The reference used 45 calls; native
60 used 90. Real elapsed times were 1,501,085 and 1,501,001 microseconds,
respectively. That is approximately 30 original timer ticks per real second
in both modes, with twice as many updates in native 60. The existing fractional
step correction is working for this short active countdown window.

The full entry-to-shutdown windows remain reported: reference 63 ticks over
2.536 guest seconds, native 64 over 2.586. Those windows include quest startup
and shutdown and do not maintain the interior update cadence. They must not be
presented as sustained timer-rate measurements. Both traces end at 89,936.
Inspected captures show the same base camp; the native endpoint reports 59.9
game FPS and 100% speed. This is one endpoint, not sustained quest performance.
The introductory time-limit announcement is visible in the reference endpoint
but absent in the native endpoint, so its display timing needs a separate
measurement. This result does not validate every quest or interface timer.

The final compact-menu reference run `quest-clock-20261010-040058-baseline`
also entered the quest and closed normally in 33.8 seconds, retaining save
hashes. Its four interior samples again used 45 calls and 45 countdown ticks
over 1,501,470 guest microseconds (1,491,679 real microseconds). The runner now
reports both the complete observation window and the interior window, and
rejects a requested native run if native hooks were not actually activated.

The guarded final research build passed all 25 regression checks and is
preserved as `FreedomUnite-Native60Research-20261010-040548`. Its follow-up
`quest-clock-20261010-040433-native60` reached the quest and restored pacing,
but the 55-second watchdog allowed only two samples before stopping it. Save
hashes were unchanged. That truncated run does not pass the runner's observation
criteria or supersede the earlier completed comparison. Route duration varies
with loading and host workload; automated quest-entry reliability still needs
improvement. No countdown rate correction was added in this pass.

The normal executable was restored with native hooks compiled out, passed all
25 checks and is preserved as `FreedomUnite-20261010-040916`. Its bounded
copied-data startup `native-comparison-20261010-040816/native60` closed normally
in 23.5 seconds, retained save hashes and still measured 30 game FPS at 100%
speed despite the research flag. The inspected language-menu capture establishes
boot behavior only. Final audits of both packages passed with 309 manifest
files each and no missing libraries, invalid files, stale source hashes or
forbidden tracked game data. Original saves and prototype baselines remain
untouched; test audio remained at 20%.

## Quest announcement timing

The entry observer at USA `08823A50` reads the twenty 36-byte message slots
at root + 3624. It records active/kind/phase/substate, the hold counter at +24,
stored slide rate at +4, position at +16 and text allocation at +32, with guest
and monotonic real timestamps. Unused slots are omitted; tracked slots retain
their transition to inactive. The supported instruction is checked at runtime.
An initial exit observer was removed: the queue loop advances its saved pointer
by 720 bytes, so treating that pointer as the original root sampled unrelated
memory. The initial reference's exit observations must not be used.

`quest-clock-20261010-120120-native60` closed normally in 51.9 seconds with
unchanged copied saves. The introductory announcement's stable hold fell from
43 to 13 ticks in 500,490 guest microseconds (498,824 real microseconds): twice
the original rate. The initial reference `115742-baseline` closed normally in
46.3 seconds with unchanged saves; valid entry samples fell from 35 to 20 and
20 to 5 over 500,490 guest microseconds each. Its invalid exit samples were
discarded. Earlier native run `115951-native60` also captured the full message
but reached the 55-second watchdog; it is diagnostic evidence, not acceptance.
Startup stalls make complete announcement duration differ from the steady hold
rate, so these are reported separately.

The guarded research correction halves only the consumed slide-in/out increments
(`0885A7E0`, `0885A8A8`) and the integer hold decrement (`0885A838`). Stored rates
and the 60-tick duration remain unchanged. Setup at `0885A120` resets a per-slot
fractional carry; cleanup at `0885A8F0` releases that slot's tracker. All five
instructions were measured from the local supported ELF and have runtime guards.
The observer and runner support a longer active measurement window while retaining
an under-one-minute watchdog, copied saves and 20 percent test volume. This does
not yet establish the timing of every message kind, queued message sequence,
pause/resume or final cleanup phases.

After the reported PC crash, source changes and the completed build were intact.
The first post-change reference (`120723-baseline`) failed navigation: late startup
confirm presses overlapped the live walking route and opened the farm menu. The
runner now waits for the final startup input action before starting that route.
Those confirms remain necessary for the slower native character-selection path;
removing them left `124100-native60` at the character-selected confirmation screen.
Normal runs omit intermediate dialogue screenshots; `-Explore` retains them and
adds north-path captures. The watchdog is now 58 seconds, retaining time to stop
only the owned process within the project's one-minute bound.

Focused `MHFU_TIMING_QUEST_MESSAGES_ONLY=1` tracing skips unrelated animation,
blend, fade and input samples, retaining the village-readiness actor sample,
quest countdown and message observations. It changes diagnostic collection only.
`124615-baseline` closed normally in 49.3 seconds with unchanged copied saves and
ten paced countdown samples. Its message held for 60 ticks across 60 updates and
2,001,960 guest microseconds (1,998,501 real microseconds). All three north-path
captures showed continued travel; this part of the route is not an idle wall wait.

Corrected native runs `124229-native60` and `124818-native60` reached the quest,
but reached the 58-second watchdog with only two countdown samples. The first
message position was 21.333334 versus the reference's 42.6666679, with the same
stored slide rate; the hold initialized to 60 and the next substate reached the
same position of 120. These observations confirm that the new consumers execute,
but do not validate the corrected complete hold rate or slide-out duration.
Other post-restart runs timed out before quest entry. All retained copied save
hashes. Runtime acceptance of the correction remains pending; no sustained native
performance, pause/resume, queued-message or combat claim follows from these runs.

The research candidate is preserved as
`FreedomUnite-Native60Research-20261010-125243`. All 25 regression checks passed
after extending the synthetic instrumentation fixture to require each of the
five message sites and reject missing or shifted instructions. The first suite
run failed because its former gate-only fixture omitted the newly required
message instructions; that failure was repaired, not ignored.

The normal executable was restored with native research hooks compiled out,
passed all 25 checks (22.98 seconds), and is preserved as
`FreedomUnite-20261010-125355`. Copied-data startup
`native-comparison-20261010-125426/native60` closed normally in 24.4 seconds,
retained save hashes, and measured 30 game FPS at 100% speed despite the research
runtime flag. The inspected capture shows the language menu, establishing startup
only. Final package audits passed for both packages (309 files each), with no
invalid manifest files, stale source hashes, missing required libraries or
forbidden tracked game data. No game process was left running. Original saves
and prototype baselines were not written; test audio remained at 20 percent.

## Announcement hold and slide runtime verification: October 10

The full-native walking run `131105-native60` still exhausted the 58-second
watchdog before quest entry. Removing navigation captures and reducing the
copied diagnostic window to scale 1 did not resolve this by themselves. A
sprint trial (`130929-baseline`) stopped short and interacted with Nekoht instead
of the Chief; its route was discarded. These are failed navigation trials,
not announcement acceptance or evidence about sustained playable performance.

The runner now offers optional `-BaselineNavigation`. In native research builds,
`MHFU_NATIVE60_BASELINE_NAVIGATION=1` requires both diagnostic navigation and
timing tracing to be enabled. All timing signatures are still validated at
startup, but hooks remain inactive during navigation. The existing countdown
entry hook activates them only when the primary countdown is positive, the
quest state is 2, and the pause flag is clear. Normal kernel pacing is restored
by the observer immediately before this activation. The runner requires the
activation log when checking this mode, including with an older executable that
does not support the option. Standard builds cannot activate it. This isolates
quest measurements; it does not validate native village or loading behavior.

`131301-native60` completed normally in 53.57 seconds and retained copied save
hashes. Its final inspected capture shows the quest base camp at approximately
60 game FPS and 100 percent speed. The matching hooks-off run `131409-baseline`
completed normally in 45.20 seconds with unchanged saves and an inspected base
camp capture at 30 game FPS. Both used scale-1 windows, scale-1 internal
resolution, normal pacing during observations, and 20 percent audio volume.

| Measured introductory announcement | 30 FPS reference | Native 60 FPS |
| --- | ---: | ---: |
| Hold countdown ticks | 60 | 60 |
| Hold update calls | 60 | 120 |
| Hold guest seconds | 2.001960 | 2.002960 |
| Hold real seconds | 2.001789 | 2.002424 |
| Slide-out update calls | 3 | 6 |
| Slide-out guest seconds | 0.101098 | 0.099098 |
| Slide-out real seconds | 0.099915 | 0.100331 |

Stored slide rate remained 42.6666679 in both modes; its consumed initial
position step was 42.6666679 versus 21.333334. The slide-out endpoints were
-8.00000763 versus -8.00001144, a small floating-point accumulation difference.
The complete hold and slide-out now have runtime evidence. The two final
cleanup phases each still take one update in both modes, making their combined
transition about 33 milliseconds shorter at native 60 FPS. They have not been
compensated. Pause/resume, queued messages, other announcement kinds, and combat
remain unverified.

The quest countdown interior rates also agree: 29.9354 ticks per real second
in the reference and 29.9375 in native mode, with approximately twice as many
native update calls. These are short base-camp measurements. The diagnostic
results now include per-announcement complete hold measurements; reused slots
are paired with their own subsequent transition rather than combined across
announcements. Exploration keeps route captures; normal measurement runs omit
them while retaining quest-entry and final captures. Installation settings and
original saves remain untouched.

The verified diagnostic candidate is preserved as
`build/windows/packages/FreedomUnite-Native60Research-20261010-131547`.
All 25 regression checks passed in 24.48 seconds. The normal executable was
restored with research hooks compiled out, passed all 25 checks in 23.57 seconds,
and is preserved as `build/windows/packages/FreedomUnite-20261010-131714`.
The final package audit passed for both, including source freshness and required
library presence (304 PE binaries checked, 298 overlay libraries). This audit
does not verify export symbols or graphics-driver compatibility.
Standard copied-data startup `native-comparison-20261010-131737/native60`
closed normally with unchanged save hashes and measured 30 game FPS at 100
percent speed despite the native runtime flag. Its inspected language-menu
capture establishes startup only. No game process remained running.

## Pause, cleanup and focused combat handoff: October 10

The two final announcement phases now consume half ticks at `0x0885A8E4`
and `0x0885A8F0`. The latter defers cleanup before touching the stack, active
flag, owned text or tracker lifetime, returning to the original caller for the
intermediate update. Both instructions retain runtime signature guards and
mandatory instrumentation fixtures. `MessageClock` keeps independent hold and
cleanup carries: a queued interruption on an odd native update cannot donate
its pending half tick to cleanup. Regression cases exercise complete and
interrupted holds, phase durations and slot restart.

The message pause predicate at `0x088567B4` reads the state object's byte
`+0x6AF1A`, distinct from the countdown routine's guard at `+0x422`. The earlier
message log's `pause` field incorrectly used that smaller guard. It now reports
the actual message pause predicate, with measured instruction guards, and
logs pause and queued-count transitions. The countdown's own log keeps its
routine-specific guard. Opening the menu alone does not pause the quest;
selecting its Pause option does. During actual pause the outer game logic skips
countdown calls, while message-root observations continue with the hold frozen.

Initial navigation tests selected Advice instead of Pause (`132906-baseline`)
or exhausted the bounded run before completion (`133400-baseline`). A temporary
opt-in pause input driver was investigated, but the successful trials did not
use it: there were no stalled-frame probe events or driver interventions. It
was removed. Those earlier trials do not establish a normal controller bug.

Final trials used the original input path with no pause driver:

| Announcement hold during actual Pause | Reference `134741-baseline` | Native `134607-native60` |
| --- | ---: | ---: |
| Normal exit; copied saves unchanged | Yes; 47.66 seconds | Yes; 52.48 seconds |
| Hold ticks | 60 | 60 |
| Active updates | 60 | 120 |
| Active guest seconds | 2.000960 | 2.001960 |
| Active real seconds | 1.997671 | 2.003300 |
| Paused guest seconds | 1.267908 | 1.251225 |
| Paused real seconds | 1.267304 | 1.251828 |
| Samples while paused; counter changed | 3; no | 3; no |

Inspected captures show the game's Paused screen followed by resumed base-camp
gameplay. The native hold remained at 19 during its paused interval. Its final
cleanup traversed phase 2 at root call 207, phase 3 at call 209, and inactive
at call 211: four native updates and 66,732 guest microseconds, matching the
reference's two updates. Slide-out remained six native updates and about
0.10 seconds. Pause-inclusive clock rates are deliberately lower; they must
not be interpreted as a new speed defect. Hold results now separate root
observations, active updates, paused time, and active time.

The queued-count field remained zero in these trials. Actual overlapping or
queued game announcements are not runtime-verified; only the independent
cleanup carry has interrupted-hold regression coverage. Combat, hit-stop,
monster behavior, quest completion and broader timing still require gameplay
comparison. The current handoff is a focused research playtest, not adoption
of native 60 FPS as the default.

`Native60-Research.cmd` launches the newest research package on a fresh copy
of the original save. `Native30-Reference.cmd` provides the matching hooks-off
comparison. Both start with interpolation disabled, fast loading and
unthrottled mode off, 1x internal resolution and 20 percent volume. Resolution
can be raised afterward for a separate performance comparison. The normal
launcher retains the standard build. See [the playtest checklist](NATIVE_60FPS_PLAYTEST.md).

The handoff candidate is preserved as
`FreedomUnite-Native60Research-20261010-134942`, with all 25 regression checks
passing in 26.22 seconds. The restored standard build is preserved as
`FreedomUnite-20261010-135117`, with all 25 checks passing in 24.89 seconds.
Final package audits passed, with 304 PE binaries checked, 298 overlays, fresh
source records, and no missing required libraries or forbidden tracked game
files. This remains a file-presence audit rather than export-symbol or driver
compatibility verification. Both manual preparation paths verified their
package manifests and produced copied saves identical to the originals.
Standard startup `native-comparison-20261010-135153/native60` closed normally,
retained copied save hashes, and showed 30 game FPS at 100 percent speed despite
the research runtime flag. The inspected capture shows the language menu;
this is startup coverage only. No game process remained running.

## October 10: failed gameplay validation and event-window investigation

The manual candidate `FreedomUnite-Native60Research-20261010-134942` failed
gameplay validation. The player reports fast climbing, jumping/falling,
gathering/carving item acquisition, NPC dialogue icons and the quest-cleared
icon. Visible swings sometimes fail to produce contact or damage; the player
clarified that the swing itself starts. These are acceptance failures even
when GAME is near 60 and SPD near 100 percent. The standard prototype remains
the reference.

The recorded manual session exited normally after 546.65 seconds. Its hunter
animation-consumer and root-motion samples show compensation active, including
steps 1.0 and 1.25 for original stored rates 2.0 and 2.5. There were 470 omitted
action-probe samples and zero invalid samples. Omitted diagnostic samples are
not evidence of dropped attacks. The current per-second action reports retain
the last action ID and do not identify exact action transitions or damage
outcomes. No crash was reported by this session's normal window-close exit.

The supplied 322-second recording is sampled at 30 FPS and covers Snowy
Mountains movement and hammer combat. Its recording rate limits inspection of
individual native updates. It does not by itself establish numerical speed
ratios, nor does the overview establish every reported village or completion
effect. Local screenshots and log summaries remain in the ignored investigation
folder, not Git.

Static inspection found a missing main-image animation-event consumer:
`0x08864368` loads channel rate +20, and `0x08864370` adds it to current frame
+16. The routine tests target membership in that upcoming interval, with
separate blending and loop-wrap branches. Gameplay overlay callers pass
actor +128 and channel index zero, so this is the same channel whose advancement
is already compensated. Using original step 2 while advancing by 1 makes
ordinary intervals overlap: a target at 3.75 matches both [2,4) and [3,5).
The research hook now scales this read using the existing animation-step
helper, preserving stored rates and the routine's branches. Original USA
instruction words `C4600014` and `46001800` were verified from the local ELF.
This corrects a demonstrated interval inconsistency; it does not establish
the cause of the missed damage or fix the other reported speedups.

Additional observation at `0x08864370` records per-caller/channel cadence,
effective step and ordinary interval matches compared with the original rate.
The comparison deliberately excludes blending and end-wrap cases. It is
bounded to 64 caller/channel entries per reporting interval and enabled only
by the timing probe. Runtime comparisons and build validation follow below.

The updated research package is `FreedomUnite-Native60Research-20261010-153932`;
all 25 automated checks passed in 23.85 seconds. Runtime signatures passed.
Reference startup `quest-clock-20261010-154114-baseline` closed normally in
39.85 seconds with unchanged copied saves. The first native observation
`153949-native60` reached the active countdown but exhausted the 58-second
watchdog before normal exit. Its observation commands also used `pad` instead
of the supported `axis` syntax; it is not a movement-validation pass.

The shorter corrected run `154204-native60` closed normally in 40.20 seconds
with unchanged copied saves. After native activation, channel `0x090B3600`
had effective event-query step 1.0. At caller `0x088D26A0`, 104 observations
yielded two ordinary-interval matches versus four hypothetical matches with
the original unscaled step. At caller `0x09C55F98`, 78 observations yielded
one versus two. These counts demonstrate overlap in the old query over the
observed frames; they do not identify a damage event or prove the combat fix.
The countdown advanced 46 ticks in 1.518 guest / 1.526 real seconds, with 91
updates. The observation is short and its capture retains navigation in the
rolling FPS statistics, so it is not a settled performance benchmark.
The capture was inspected. Baseline navigation deliberately keeps native
hooks inactive until the quest: these runs do not validate native village
movement, NPC effects or loading behavior.

Next investigation: locate independent movement/vertical integration, action
completion and item-award counters; inspect village/quest HUD overlay clocks;
trace hit activation, collision and damage separately from visible poses.
Main-image announcement timing tests cover a different subsystem from the
reported NPC and quest-cleared effects. No blanket halving of shared state or
unverified overlay edits is justified by these findings.

## October 10: separate movement diagnostics

An optional `MHFU_TIMING_PLAYER_STATE=1` trace now records the measured USA
hunter object's action changes, animation frame, position, raw movement and
acceleration fields, and the independent counter at +0x414. It observes values
without changing them. Ordinary state samples are limited to four per guest
second plus action changes; nonzero movement vectors receive up to 64 additional
samples per guest second so short flight phases can be inspected. The quest
runner enables this with `-PlayerMotion`; the research manual wrapper enables
it as well. These addresses are measured for this executable and copied save,
not a general player-object discovery mechanism.

Grounded comparison `quest-clock-20261010-160456-baseline` and
`quest-clock-20261010-160733-native60` used the same two-second walking input.
Both closed normally with unchanged copied saves (48.56 / 48.88 seconds).
Their stop action lasted 633,954 / 617,271 guest microseconds across 19 / 37
updates, a difference of one native update. Horizontal endpoints differed by
approximately 6.8 world units. Input phase and collision handling prevent a
claim of exact positional equality. Inspected captures show the hunter near
the camp tent; all sampled independent movement vectors were zero. This
supports the already adjusted grounded path, not jumping, falling or combat.

A second reference route, `quest-clock-20261010-161106-baseline`, closed
normally in 46.41 seconds with unchanged copied saves. Its inspected capture
shows the hunter on a slope beside a rock. It also remained grounded and did
not exercise vertical integration. These attempts do not validate flight.
The matching native run `quest-clock-20261010-161306-native60` closed normally
in 55.12 seconds with unchanged copied saves; its capture shows the same slope.
The horizontal endpoints differed by about 38 world units after three seconds
of input against the rock. Collision and input phase have not been isolated,
so this does not establish equivalent movement in contact with terrain.
Regression checks ran during that lap; its wall time is not a performance
comparison. The new dense flight sampling branch remained unexercised because
the observed movement vectors were zero.

Static inspection of local gameplay overlay routine `0x09A905F0` found a
separate counter increment at `0x09A90604`, movement-plus-acceleration updates
at `0x09A90704`, `0x09A90714`, `0x09A90724`, and position-plus-rotated-movement
updates at `0x09A907B8`, `0x09A907C4`, `0x09A907D0`. This independent integration
has no native time-step correction. Runtime action mapping and matching
flight traces are still required before changing it; halving only gravity
would leave position integration wrong. Other weapon/action routines contain
parallel movement paths, while many position stores are collision corrections
or floor clamps and must not be indiscriminately scaled.

Lobby and gameplay overlays also contain raw frame-counter consumers beyond
the corrected main-image HUD paths. Some mask the counter for sprite effects;
others deduplicate callbacks and must retain their scheduling semantics.
The exact NPC icon and quest-cleared consumers have not yet been identified.
No generated overlay files were edited, and no physics or HUD fix is claimed.

The diagnostics are preserved in research package
`FreedomUnite-Native60Research-20261010-161417` (25 checks passed, 26.07 seconds).
The normal build was restored with native hooks compiled out and preserved as
`FreedomUnite-20261010-161535` (25 checks passed, 24.90 seconds). Both package
audits passed with 309 files each and no invalid entries or missing dependencies.
This remains a file-presence audit with the limitations stated above.
Standard startup `native-comparison-20261010-161550/native60` closed normally
in 24.65 seconds with unchanged copied saves. Its inspected language-menu
capture shows GAME 30 / SPD 100 even with the runtime research flag enabled;
this verifies startup isolation, not a complete gameplay regression. No game
process remained running. Manual native acceptance remains failed, and the
new package is diagnostic rather than a claim that the reported issues are fixed.

## October 10: gameplay overlay action routing

Optional read-only overlay probes are now compiled into local copies of five
units of the content-identified USA `game_task` overlay, hash
`36BD79C16BF70197`. Original generated files are preserved. CMake refuses an
absent target/unit, and the existing instrumentation script refuses missing
or ambiguous labels. These observations do not alter any game registers or
memory. They run only with timing plus player-state tracing enabled, are
restricted to the measured hunter object, and emit at most 64 samples per
point per guest second. Supported points cover the category dispatcher,
two action dispatchers, and two independent flight entries. This is not a
physics correction or a generic hook for every overlay variant.

The category dispatcher at `0x09A675AC` uses actor byte +0x298 to select a
handler. Reference run `quest-clock-20261010-162916-baseline` actually observed
category zero routing through `0x09A675B8`, which calls `0x09A9A9E0`. That
normal-action dispatcher uses actor byte +0x299 and a 128-entry table, unlike
the earlier 40-entry dispatcher at `0x09A8CDC0`. The earlier action/flight
probes did not fire for this hunter's grounded normal movement. This is why
the first inspected vertical routine is not yet an established fix target.
The measured +0x1E8 value was 4; no weapon-type meaning is asserted here.

The normal dispatcher and its independent flight entry `0x09A9C530` now have
additional observations. The flight routine increments +0x414 at
`0x09A9C54C`, adds acceleration to velocity at `0x09A9C670`, `0x09A9C680`,
`0x09A9C690`, and later adds transformed movement to position; `0x09A9C730`
is a Y-position integration instruction, not the routine's entry. These
per-update paths still require runtime flight evidence and time-step work.

Player-state reports now include +0x299 transitions and callback +32. A
one-time raw callback-table dump helps classify callbacks without assuming
their purpose. Village observation identified `0x088D6108` at +32; static
inspection shows this is an animation-related callback, not the overall
action dispatcher. Village overlay addresses cannot be interpreted as
gameplay code merely because the overlays share a load base.

Additional reference traversal runs `161915-baseline`, `162215-baseline`,
and `162916-baseline` closed normally in 46.04, 54.17 and 55.99 seconds with
unchanged copied saves. None exercised independent vertical motion. The
callback-table village run `162656-baseline` closed normally in 30.82 seconds
with unchanged copied saves, but the quest script correctly reported failure
because no quest countdown was reached; it is callback evidence, not a
quest-clock pass. No jump/fall equivalence or combat fix is claimed.

Native run `quest-clock-20261010-163255-native60` closed normally in 53.30
seconds with unchanged copied saves. Its inspected capture shows GAME 60 /
SPD 100 on the camp slope; it remained grounded. The category and confirmed
normal dispatcher produced matching observations: 120 updates in state 1
route to `0x09A9AA24`, 38 in state 44 route to `0x09A9ACC4`, and 56 in state
zero route to `0x09A9AA14`. State 44 accompanies the visible stop animation
in this run. These are dispatcher observations, not damage events, and do not
validate the flight-entry probes. The targeted manual comparison now asks
for the same climb, ledge jump/drop and gathering point in reference/native
copies, with one carve if convenient, rather than requiring a complete hunt.

The updated candidate is preserved as
`FreedomUnite-Native60Research-20261010-163445` (all 25 checks passed in
24.76 seconds). The standard executable was restored with native hooks
compiled out and packaged as `FreedomUnite-20261010-163558` (all 25 checks
passed in 25.18 seconds). Both final package audits passed with 304 PE binaries,
298 overlays, 309 files per package, fresh source records, and no missing
dependencies or invalid package entries. Audit scope remains file presence,
not export-symbol or driver compatibility. Both manual preparation paths
verified their package manifests and made isolated save copies; no manual
game session was started by preparation. Native gameplay acceptance remains
failed; the next manual run is specifically to collect the missing flight and
interaction evidence, not to certify that those issues are fixed.
Standard startup `native-comparison-20261010-163622/native60` closed normally
in 24.49 seconds with unchanged copied saves. Its inspected language-menu
capture shows GAME 30 / SPD 100 despite the runtime native flag; this is
startup isolation coverage only. No game process remained running.

## October 10: paired manual traversal and combat traces

The player completed the same sequence in reference session
`manual-research-20261010-235024-564` and native session
`manual-research-20261010-235315-509`: enter a quest, jump from the starting
ledge, kill a Popo, carve twice, climb the short and high ledges, run and jump
from the latter, gather herbs, then quit. Both used research package
`163445`; the reference runtime flag was off. Both closed normally with exit
4 after 147.70 / 150.96 seconds, and both audio totals report zero dropped
frames. These are human-driven runs, not the bounded automated laps.

The flight-entry probe now fired in both modes. On the second drop, both
traces start at Y=500, end their observed flight at Y=20 and counter +0x414=32,
with 34 recorded flight-entry samples. The observed interval is 1,101,078
guest microseconds in reference and 550,539 in native: exactly half. Gravity
still changes stored Y velocity by -2 on every update, at approximately
33,366 / 16,683 microseconds respectively. This demonstrates the independent
integration speedup rather than merely an animation impression. Horizontal
starting positions differ, so exact spatial equivalence is not asserted.
The first drop also shows unchanged per-update gravity, but its start heights
differ and cannot support a like-for-like total-duration comparison.

Two carving action-state 74 intervals each last 5,805,684 / 5,772,318 guest
microseconds. Herb interaction state 71 lasts 4,170,750 / 4,137,384; its
subsequent state-93 cycles are approximately 3,736,992 / 3,720,309. Main
climb-pull state 58 lasts 2,202,156 / 2,168,790 in both climbing sequences.
Those differences are one or two native updates, unlike the flight speedup.
They measure observed state occupancy, not the exact instant an item was
awarded. Climb approach/preparation states differ more, and initial position
and input have not been isolated from their timing. Overall climbing is not
declared correct on the strength of the pull animation alone.

Combat takes category one with +0x1E8=4, routing through `0x09A8AF48` to
weapon dispatcher `0x09BA7EF0`. Both runs show attack animations 1401, 1402,
1404, 1405 and 1406. The native trace repeatedly completes state-2/animation
1405 intervals; this establishes that attacks start and advance, not that
each contacts or damages the monster. The reference sampled six zero-rate
weapon-animation states among 91 samples; native sampled none among 172.
This is a possible contact/hit-stop discrepancy, not proof of missing damage:
the sparse ordinary state probe can miss short freezes, and inputs, aiming
and monster positions were not identical. The player's report of killing
the Popo shows that some attacks dealt damage, not that all expected swings
registered. Current probes do not log monster HP, collision acceptance or
damage application, so attack registration remains unresolved.

Ignored `manual-comparison-20261010.json` records the raw state/flight episode
measurements and the analysis script. No binary was rebuilt or modified while
the player ran the native comparison. Next work can now target the proven
flight integration path, followed by climb preparation, while separate
contact/damage observations are needed to assess the reported missed swings.

## October 11: early carving and gathering awards

The player clarified that item awards still happen early at native 60 FPS.
In reference carving, the item arrives after the first knife motion finishes;
in native carving, it arrives at the beginning of that motion. Gathering also
awards items early according to the player. This is a confirmed observed
acceptance failure, even though the overall recorded action-state durations
are close. Those duration measurements do not validate reward timing.

The shared normal interaction handler `0x09AA0E08` serves the observed state
71 and state 74 entries with different arguments. Its item-award trigger must
be traced independently of animation completion and total state occupancy.
The timing data does not yet establish whether a frame threshold, discrete
counter, or another event consumer causes the early award; no cause or fix
is claimed. Carving/gathering remain open alongside flight, climb preparation,
HUD effects and attack registration.

## October 11: Yakumo 60 FPS implementation comparison

Checked the installed Yakumo v0.6.0-alpha.4 release against public source at
commit `692e9b29cecd7cbbba95d5b0defaa6d78a2a797e`. Its README and
`frame_interpolation.hpp` explicitly keep game simulation at 30 FPS. The
60 FPS setting requests extra renderer presentations, not extra simulation
updates. A native executable does not imply native 60 FPS game logic.

The renderer records two consecutive game frames, matches perspective draws,
and replays the older frame with transforms blended toward the newer one.
Animated models blend bone matrices on the raw GPU skinning path or positions
and normals of already-skinned vertices on the CPU-decoded path. These are
display-only intermediate poses; they do not advance animation events,
inventory awards, collision checks or damage logic. Interface/orthographic
draws are held instead of blended. Camera/scene discontinuities and implausible
object motion have rejection guards. Presentation pacing divides the requested
display rate by the 30 FPS game rate.

FU already contains this interpolation architecture in its framework. It is
not a new missing native-60 animation correction. Yakumo's implementation
therefore supplies no drop-in fix for FU's doubled flight integration, early
item awards or unresolved attack registration. Those still require inspection
of the game-specific simulation consumers and event ordering. Keeping an
unchanged 30 FPS reference remains essential when evaluating those changes.

This was source inspection, not a Yakumo runtime benchmark. Selected exact
release files were saved in the ignored investigation folder. No game code,
build, installed Yakumo settings or saves were changed by this comparison.

Primary sources pinned to the installed release:

- [Yakumo README](https://github.com/TeamGDB/Yakumo/blob/692e9b29cecd7cbbba95d5b0defaa6d78a2a797e/README.md)
- [Interpolation definitions and guards](https://github.com/TeamGDB/Yakumo/blob/692e9b29cecd7cbbba95d5b0defaa6d78a2a797e/profiles/mhp3rd/host/gpu/frame_interpolation.hpp)
- [Renderer pose blending](https://github.com/TeamGDB/Yakumo/blob/692e9b29cecd7cbbba95d5b0defaa6d78a2a797e/profiles/mhp3rd/host/gpu/vulkan_renderer.cpp)
- [Presentation pacing](https://github.com/TeamGDB/Yakumo/blob/692e9b29cecd7cbbba95d5b0defaa6d78a2a797e/profiles/mhp3rd/host/gpu/frame_pacing.cpp)

## Research references

The community's [EUR FU patch author post](https://forums.ppsspp.org/showthread.php?tid=22800&pid=133521)
shows that existing attempts modify more than a frame limiter. Subsequent
[discussion reports hitbox problems](https://forums.ppsspp.org/showthread.php?tid=4799&page=54).
These reports guide questions, not verified results for our USA version. No
community patch code was copied or installed. Addresses and behavior above
were checked independently against the local supported executable.
