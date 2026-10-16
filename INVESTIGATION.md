# Interpreter coverage investigation — 6 October 2026

## Texture mod foundations — 9 October 2026

Added a port-owned Mods page through an optional generic launcher callback.
It uses existing texture replacement settings/importer and offers original-art
fallback. Inspected automatic launcher captures confirm navigation and setting
persistence. A separate authoring tool composes compatible flat PNG packs into
new output, with deterministic later-layer overrides, disabled-key fallback,
deduplication, strict option/path/format checks, and source preservation.

Six synthetic-art cases pass in a new CTest target; the complete suite is now
22/22 passing. A two-key composed fixture passes the native importer review.
The rebuilt strict-native game reaches the inspected language screen near
30 FPS and closes normally. The latest package is `FreedomUnite-20261009-190146`;
both it and the preserved smooth-Jungle package have all 309 recorded files
verified. No real HD/HUD artwork or gameplay mod was installed. See
[TEXTURE_PACK_WORKFLOW.md](TEXTURE_PACK_WORKFLOW.md).

## Project audit and fullscreen restoration — 9 October 2026

The native executable and all 298 overlays were audited for imported DLL file
availability: 304 PE binaries checked, no missing libraries or parse errors on
this machine. All 309 recorded files of the preserved smooth-Jungle package
match their hashes. The supported ELF and current executable metadata match.

Fullscreen now explicitly restores cached window geometry after the SDL state
transition. A bounded real-launcher test, with inspected captures, verifies
960×544 → 1920×1080 → 960×544. Maximized/multiple-display behavior remains a
broader validation task. The framework patch contains this generic fix.

Test execution now isolates inherited playtest variables. The first audit run
failed its interpreter test because the preceding strict playtest left fallback
disabled; all 21 tests pass after environment isolation. Packaging now hashes
all local host/workflow sources and rejects post-build source changes. The
negative stale-source check passed. Package instructions no longer state that
interpreter fallback is required.

The mod coding plan includes smooth analog camera plus press-L target snapping,
with actor validity, original item controls, texture composition, and QoL
dependencies. No gameplay mods or native 60 FPS behavior are enabled. See
[PROJECT_DIAGNOSTICS.md](PROJECT_DIAGNOSTICS.md) and
[MOD_IMPLEMENTATION_PLAN.md](MOD_IMPLEMENTATION_PLAN.md).

## Native 60 FPS research started — 9 October 2026

The USA frame gate is identified and an optional observation-only timing probe
has been built and checked. Native gameplay remains at 30 FPS. Bounded menu
traces show roughly 60 vblank callbacks leading to 30 gate wakes and 30 flips
per second. All 21 tests pass. The subsequent 177.8-second manual village-to-hunt
session confirms the same cadence in gameplay and ends normally on window close.
Static analysis of the main executable and all 298 recovered overlays identifies
a countdown caller that explicitly subtracts one per update; the generated
internal call bypasses the current outer-dispatch observer. Simulation timing
mapping is still required before implementing an experimental native 60 FPS mode.
An optional diagnostic build now adds three internal observation points using
build-directory copies of two generated units. Original guest instruction text
is preserved, all 21 tests pass, and a bounded rendered check reaches language
selection at 30 FPS and closes normally. Hunt action measurements are pending;
native 60 FPS remains disabled.
See [NATIVE_60FPS.md](NATIVE_60FPS.md) for findings and verification limits.

## Jungle CPU geometry candidate — 8 October 2026

The latest Jungle logs show CPU-side graphics work exceeding the frame budget
while GPU time is low. An x86 SSE2 optimization now performs weighted bone
transformations across three axes together without changing scalar arithmetic
order. GPU vertex decoding stays disabled; its reported tree corruption remains
unresolved.

The candidate builds successfully. All 21 regression tests passed, and expanded
decoder tests match the general implementation bit for bit in 23,040 synthetic
cases. Paired uncontended eight-bone benchmarks are 1.99–2.21 times faster than
the previous CPU path. This is not a measured Jungle FPS gain.
`MHFU_NO_SIMD_SKINNING=1` restores scalar skinning for comparison. Bounded
rendered checks used copied saves and reached the language screen and opening
movie. They did not establish native 3D gameplay coverage. The preserved
baseline and existing validated packages remain intact. Evidence is under
`build/windows/investigation/jungle-cpu-skinning/REPORT.md`.

The 9 October manual test restored visible geometry, but the user still reports
frame drops and audio lag in Jungle areas 2 and 3. Other areas were not checked.
The corresponding log (`manual-native-20261009-074859`) shows about 36–38 ms of
geometry decoding per game frame, roughly 41 ms of CPU rendering, and typically
under 1 ms of GPU time in the slow stretches. The CPU candidate therefore has
not resolved the real gameplay slowdown. Fullscreen window restoration and
GPU-path correctness remain open.

## Yakumo release comparison — 9 October 2026

Read-only inspection of Yakumo v0.6.0-alpha.4 and its exact public source commit
`692e9b29cecd7cbbba95d5b0defaa6d78a2a797e` found the same basic CPU vertex decoder
and indexed-range decoding strategy already present in our framework. Its GPU
vertex decoder is opt-in, not enabled by default. Both renderers have reusable
draw storage, direct guest-memory access, draw merging, and background texture
decoding. Background decoding is for textures, not vertex geometry.

FU's renderer is compiled with Release `/O2 /Ob2`; this is not an accidental
unoptimized host build. Yakumo's saved internal scale is 6 versus FU's 2, which
does not support reducing resolution as the primary remedy for FU's measured
CPU bottleneck. These settings are not a controlled performance comparison.

The FU log reports approximately 7,900 draw requests versus 112 recorded GPU
draws per frame in the slow stretches. Both implementations decode geometry
before renderer merging. Indexed primitives decode the complete span from
their lowest to highest index, potentially including unreferenced vertices;
overlapping spans may also repeat work across primitives. The fast CPU path
excludes multiple morph targets. These are measurable investigation leads,
not confirmed causes in Jungle assets: the current trace does not identify
vertex formats, decoded counts, sparse spans, or repeated vertex ranges.

Next measure per-format decoding time and counts, fast-path/fallback use, and
indexed span size versus referenced vertices. Choose an optimization only from
that evidence. Any reuse must account for vertex data, format, and bone-matrix
changes; address-only caching would risk stale or corrupted geometry. Validate
against the reference decoder and the same Jungle scene before making an FPS
claim. Keep the existing GPU opt-out and preserved prototype.

Detailed comparison and pinned source references:
`build/windows/investigation/yakumo-comparison-20261009/REPORT.md`.

### Geometry measurement build — 9 October 2026

`MHFU_TRACE_VERTEX_DECODE=1` now aggregates CPU decoder work by vertex format
once per real second. Rows include decoding time, runs, vertices, bytes, fast
and general path counts/times, skinned runs, fallback reasons, and indexed
span/unique/unused counts. Layout and vertex memory resolution are timed;
index scanning and diagnostic unique-index counting are outside that timer.
The existing render split measures the wider geometry-preparation interval.
Span counts do not establish reuse across draws, and bytes are source spans,
not measured physical memory traffic. Diagnostic overhead must be considered
when interpreting FPS; this build adds measurements, not a performance fix.

The Windows Release build and all 21 tests passed. With tracing enabled,
23,040 synthetic decoder cases still match the general loop exactly, and a
synthetic benchmark emitted the new aggregate rows. Those results do not
measure Jungle workload. The manual launcher enables this trace, keeps GPU
decoding off, and clears alternating decoder switches. The user agreed to a
Jungle areas 2/3 test; those measurements remain pending. The previous executable
is preserved in `build/windows/investigation/vertex-measurement-20261009/`;
earlier complete packages and the working prototype remain preserved.

### Measured sparse indexed geometry bottleneck — 9 October 2026

The completed manual lap (`manual-native-20261009-081047`, approximately 409 real
seconds, normal window-close exit) contains 391 measurement windows. The user
reports the starting area and areas 4, 10, and 8 smooth; areas 1 and 2 worst;
other Jungle areas also affected by frame drops, audio disturbance, and delayed
controls. Area numbers are not present in the profiling log, so individual
measurement windows cannot be assigned to areas from this trace alone.

Format `0x0000111a` accounts for 195,416 of 202,467 milliseconds of recorded
decoder time (96.52%). Across the reported windows, its index spans contain
11,939,454,359 vertices, but only 195,604,721 unique vertices are referenced
within individual draws: 98.36% of span conversion is unused. Some expensive
windows convert about 45 million vertices against only 360,000 references.
This format has no skinning, and all reported decoder runs use the fast path.
The expensive work is decoding the gaps in sparse indexed spans, not a missing
fast-path format or weighted bone processing. Unique counts are per draw, not
unique geometry across an entire scene.

A CPU candidate now compacts sparse 16-bit indexed draws, sorts unique indices,
decodes contiguous runs of referenced vertices, and remaps the original index
sequence. Dense spans, other index types, multiple morph targets, and GPU raw
draws keep the existing route. No transformed-geometry cache is introduced.
Eligibility requires a span of at least 64 vertices and more than four times the
index count. `MHFU_NO_SPARSE_DECODE=1` restores the previous span path; alternating
the existing `decode` performance switch also restores it with the old decoder.

The candidate Windows Release build and all 21 regression tests passed.
Differential display-list cases compare every consumed vertex against the
reference route, including duplicate indices, shared-buffer consecutive draws,
dense draws, skinning, through mode, and six primitive types. Native Jungle
FPS, visible geometry, audio, and control responsiveness after this change
remain pending manual verification. The manual launcher enables the candidate
and retains diagnostic logs. Previous executables and prototype packages remain
preserved. Evidence: `build/windows/investigation/sparse-indexed-20261009/`.

### Successful post-fix Jungle lap — 9 October 2026

The user reports that all Jungle areas now seem smooth. The corresponding
`manual-native-20261009-083148` session ran approximately 210 real seconds and
closed normally. Sustained gameplay samples show about 29.9–30.0 FPS and 100%
emulation speed. Representative heavy-scene geometry preparation is now about
3.0–3.2 ms per game frame, compared with the earlier roughly 36–38 ms samples.
These are manual laps, not identical camera frames or a controlled benchmark.
Brief low-FPS samples remain around stage loading and other loading activity;
the whole-run minimum must not be represented as steady gameplay performance.

The user did not separately confirm audio and controls in this message; their
all-area smoothness report is recorded as stated. No independent audio continuity
test or broader game-content verification is claimed. The source change passed
the existing regression and differential geometry tests above.

The tested build is preserved separately as
`build/windows/packages/FreedomUnite-20261009-083626/`. Earlier prototype packages
remain intact. The manual launcher now uses `MHFU_PERF=overlay+log`, retaining
the top-left FPS display and diagnostic logs.

The verified gameplay prototype remains preserved. This investigation identifies
why the current generated corpus still needs the interpreter; it does not
establish that the new development build completes a hunt without it.

## Findings

### Startup code is embedded in a non-executable section

Before the startup coverage change, with `PSPRECOMP_NO_INTERPRETER=1` the development build stopped at
`0x089B93D8`, before any virtual time or vblanks have elapsed. All 272 main
executable imports are bound, with zero logging stubs in strict HLE mode.

The missed address is in `.data` (`0x0890FA18–0x089CBBB0`), whose section
flags do not mark it as executable. Although the containing PT_LOAD is
executable, automatic code generation uses executable section ranges. The
address contains a valid 20-instruction startup routine, ending at
`0x089B9428` including the return delay slot. The first failure is therefore
missing corpus coverage, rather than an unimplemented PSP import.

An isolated generation experiment used the existing recompiler's
`--code 0x089B93D8-0x089B9428` option. It added a 20-instruction unit and
registered its addresses. All 67 existing units matched the current corpus
byte-for-byte. The experiment was generated under `build/windows/investigation/`
and was not linked into the development executable.

The initial boot trace also reported interpreter entries at `0x089B95F8`,
`0x089B96B4`, and `0x089BA6B4`, inside `.data`. Their complete code boundaries
were subsequently mapped as part of the contiguous constructor block below.
Compiling the first routine alone would not remove fallback.

### The ELF records runtime overlay slots

Zero-length overlay sections and the type-0 program headers describe these
groups. Their maximum memory extents establish the slot boundaries.

| Group | Start | End, exclusive | Named images |
| --- | --- | --- | --- |
| Task | `0x09A5F300` | `0x09C19100` | 12 |
| Subtask | `0x09C19100` | `0x09D1A280` | 2 |
| Monster | `0x09D1A280` | `0x09D63100` | 17 |
| Stage | `0x09D63100` | `0x09D65500` | 267 |
| Debug marker | `0x09D65500` | `0x09D65580` | 1 |

These are ELF image markers, not proof that every image is loaded during play.
The profile previously supplied no slots, preventing the framework's existing
overlay identification and dumping from recognizing these addresses.

The profile now supplies the measured slots. Set
`MHFU_DISABLE_OVERLAY_SLOTS=1` to restore the previous empty-slot behavior for
comparison. No framework changes or generated-code replacement were needed.

### Boot loads two identifiable overlays

With `MHFU_DUMP_OVERLAYS` pointed at an ignored local directory, a bounded
rendered boot identified and dumped:

| Image | Loaded at | Code bytes | Data bytes | Dump bytes |
| --- | --- | --- | --- | --- |
| `demo_task.ovl` | `0x09A5F300` | 10,332 | 740 | 11,136 |
| `demo_sub.ovl` | `0x09C19100` | 7,632 | 2,288 | 9,984 |

Both have the `MWo3` header recognized by PortableKit. Its existing
`add_overlay.py --no-build` generated separate corpora for both, in the ignored
investigation directory. Neither generated corpus has an unsupported
instruction site within its header-declared code range. The current wrapper
also marks header and data bytes as executable, producing unsupported sites
outside those ranges; those sites are not evidence of unsupported game
instructions. Subsequent strict boot identified genuine constructors in the
overlay's data region, so restricting generation to the declared code alone
would omit required code. Any narrower wrapper must also map these constructors.

During the initial investigation the two overlay libraries were not compiled or installed. The disc archive
layout and overlays used by village, hunting, monsters and stages remain
unmapped by this investigation. In particular, the current runtime dumper
names files by slot address: another image in the same slot overwrites that
slot's previous dump. Preserve each identified image before advancing through
additional scenes, or improve dump naming first.

## Checks and limits

- Rebuilt the separate development executable after the profile-only change.
- Strict interpreter-disabled boot reproduced the first missing startup address.
- Rendered boot reached language selection; the screenshot was inspected.
- Boot with the slot map enabled and disabled reported the same interpreter
  totals: 204,164 instructions, 1,953 entries, 67 distinct entry addresses.
- Scripted window closure returned the runtime's expected exit code 4.
- Rechecked all 548 preserved baseline files: every size and SHA-256 matched.
- Generated experiments, dumps, ELF layout, disassembly, captures and runtime
  logs remain local under `build/windows/investigation/`; no game bytes are
  included in this report.

This was a boot investigation. No new full gameplay-loop test, performance
benchmark, overlay library execution or interpreter-free gameplay is claimed.

## Startup coverage implemented — 6 October 2026

The constructor-pointer table at `0x089BAB70` names seven routines. Disassembly
confirmed the following contiguous code boundaries, including return or
tail-jump delay slots:

| Start | End, exclusive | Instructions |
| --- | --- | --- |
| `0x089B93D8` | `0x089B9428` | 20 |
| `0x089B9428` | `0x089B9444` | 7 |
| `0x089B9444` | `0x089B9678` | 141 |
| `0x089B9678` | `0x089B96B4` | 15 |
| `0x089B96B4` | `0x089BAAF8` | 1,297 |
| `0x089BAAF8` | `0x089BAB34` | 15 |
| `0x089BAB34` | `0x089BAB70` | 15 |

The Generate stage now explicitly includes `0x089B93D8–0x089BAB70` and writes
generation metadata. Build verifies the supported ELF hash, recorded extra
ranges and registry hash before compiling. The added unit covers all 1,510
instructions, with no unsupported static instruction sites; the original 67
units remain byte-identical to the preserved baseline.

The new development executable compiled and linked. With the interpreter
disabled, boot passed the constructors and stopped at `0x09C1B780`, after
200 ms of virtual time and 12 vblanks. The trace identifies the loaded image
as `demo_sub.ovl`. This address starts a valid constructor in the overlay's
data region: its declared code ends at `0x09C1AF10`. A second constructor
starts at `0x09C1B7BC`; their pointer table starts at `0x09C1B7E0`.

Rendered boot with fallback enabled still reached language selection; its
capture was inspected. Compared with the same 150-frame boot script before
the change, interpreter totals decreased from 204,164 to 202,654 instructions,
from 1,953 to 1,911 entries, and from 67 to 25 distinct entry addresses. The
full 1,510-instruction startup block is therefore no longer interpreted in this
check. These are coverage measurements, not a performance benchmark.

All 20 existing tests passed. All 548 baseline files were rechecked and matched.
New runtime/build/test logs and constructor mapping remain under the ignored
investigation directory. At the end of that startup step no overlay libraries had been installed.

## Boot overlays implemented — 6 October 2026

The Overlays workflow stage now validates local dumps and generates their
corpora into the ignored profile `overlays/` tree before building. Both
identified boot libraries compiled and linked using the existing framework;
no framework edits were required. Its broad entry-point generation includes
the embedded data-region constructors needed at `0x09C1B780` and `0x09C1B7BC`.

A bounded rendered run with both `MHFU_STRICT_HLE=1` and
`PSPRECOMP_NO_INTERPRETER=1` installed `demo_sub` and `demo_task`, reached
language selection, and exited through the scripted window-close action.
The screenshot was inspected; no interpreter or unsupported-instruction
failure occurred. Exit code 4 is the runtime's expected result for this
deliberate stop.

Build metadata now records overlay DLL hashes. Packaging checks the expected
library set and hashes, copies the DLLs into the package's `overlays/`
directory, and includes them in its recursive file manifest. The local
package is `build/windows/packages/FreedomUnite-20261006-235234/`.
All 13 manifest entries, including the two DLLs, passed hash verification.
The packaged executable also reached language selection with the interpreter
disabled, installed both libraries from its own package directory, and quit
through the same script; its screenshot was inspected.

All 20 existing tests passed again. All 548 preserved baseline files still
match their recorded sizes and hashes. Captures, logs, dumps and generated
overlay code stay ignored. Native boot is now verified through language
selection; title-menu navigation and the complete gameplay loop remain to
be checked with the interpreter disabled.

## Menu and character-editor coverage — 7 October 2026

Bounded strict runs passed the opening notices and reached the game menu.
Selecting New Game unloaded `demo_task` and stopped at `0x09A5F380` in the
previously uncompiled `edit_task.ovl`. Its dump was preserved separately from
the slot-address file before further navigation could overwrite it.

The image has ID 2, base `0x09A5F300`, 31,140 code bytes, 5,148 data bytes,
and a 36,352-byte dump including its header. The existing Overlays stage
generated three units, compiled and linked the library, and recorded it in
build metadata. No framework or handwritten runtime changes were needed.

With the interpreter disabled, a subsequent run entered the character-creation
selection screen and cancellation reinstalled `demo_task` at the same slot.
The trace showed `demo_task → edit_task → demo_task`, validating code removal
and reinstallation for this transition. That longer script reached its
55-second watchdog before its scheduled quit; it was stopped and a shorter
run followed.

The shorter strict run reached the actual character editor, with a rendered
hunter model and the name/gender/clothing/appearance controls, and exited
through scripted window closure. Captures were inspected. This establishes
entry into the editor, not completion or saving of a new hunter; returning
from the actual editor was not established by that shorter script.

All 20 existing tests passed. The new local package is
`build/windows/packages/FreedomUnite-20261007-000006/`. Its 14 manifest entries
passed hash verification, including all three overlay DLLs. The packaged
executable was checked through entry into the character editor with fallback
disabled and scripted window closure. All 548 baseline files remain intact.
Logs, preserved dumps and captures are local under
`build/windows/investigation/menu/`.

## Hunter creation, saving and reload — 7 October 2026

A bounded rendered run with `MHFU_STRICT_HLE=1` and
`PSPRECOMP_NO_INTERPRETER=1` created a default male hunter named Test in slot 1
of the isolated `build/windows/game/` installation. The game's name dialog,
creation confirmations and save-slot confirmations completed. The runtime
logged an encrypted AUTOSAVE of 1,483,008 plaintext bytes; MHP2NDG.BIN on disk
is 1,483,024 bytes. The other save files are ICON0.PNG, PARAM.SFO and PIC1.PNG.
The save-complete capture shows the subsequent Start Game prompt.

The four save files and a hash manifest are backed up locally under
`build/windows/investigation/hunter/saved-test-hunter/`. The player's original
save installation was not used or changed.

Cold restart decrypted and loaded the saved data. Selecting Continue first
stopped in `cont_task.ovl`, which has base `0x09A5F300`, 2,076 code bytes,
676 data bytes and a 2,816-byte image. The Overlays workflow generated and
compiled this fourth library. Subsequent strict runs displayed Test in the
character selector, reloaded the selected hunter and started the opening
cinematic. The inspected captures and logs are under
`build/windows/investigation/hunter/`.

All 20 existing tests passed in 17.22 seconds. The new local package is
`build/windows/packages/FreedomUnite-20261007-001930/`; all 15 manifest entries,
including four overlay DLLs, passed hash verification. All 548 preserved
baseline files still match their recorded sizes and hashes. The test save,
game data, captures and generated code are excluded from the package.

To advance beyond the cinematic within the 55-second run limit, a diagnostic
run used existing unthrottled pacing, immediate presentation and internal
scale 1, with periodic screenshots disabled. The isolated settings file was
restored byte-for-byte afterwards. An earlier unthrottled run with vsync
reached its watchdog while the cinematic was active; this is not evidence of
a game deadlock or a performance benchmark.

The four-library packaged executable loaded its own libraries, decrypted the
saved hunter twice (startup and selection), and advanced to a new image in the
task slot. It stopped at `0x09B0FE00` with no recompiled function registered:
`lobby_task.ovl` has ID 7, base `0x09A5F300`, 665,728 code bytes, 58,560 data
bytes, 80,896 BSS bytes and a 724,352-byte dump. The preserved image is
`build/windows/investigation/hunter/overlays/lobby_task.bin`, SHA-256
`A2E844B86AB5A753C2D8A27D7EA1507B90BE5453DCB434215B05F52AB616DDD1`.
The run ended at this code gap before its scripted quit; exit code 4 describes
the strict stop. It does not establish village rendering or movement.

## Lobby library and next gameplay dependency — 7 October 2026

The Overlays stage generated `lobby_task` into 45 units and compiled
`ovl09A5F300_lobby_task_F431CAA134157E88.dll`. No handwritten runtime or
framework changes were needed. A bounded strict rendered run reloaded Test,
installed the new library and passed `0x09B0FE00`, the previous stopping point.

The game then replaced `demo_sub` with `game_sub.ovl` in slot `0x09C19100`.
With fallback disabled it stopped at the first missing entry, `0x09CFB680`.
The image has ID 14, 745,664 code bytes, 181,888 data bytes, 125,440 BSS bytes
and a 927,616-byte dump. It is preserved as
`build/windows/investigation/hunter/overlays/game_sub.bin`, SHA-256
`9110F1E10FD46E1F12568B18064B8E05E4903B95DF595D1A8B256DFE2248AFF7`.
This establishes native lobby-library installation, not village rendering or
movement. The run stopped before its scheduled capture or quit.

All 20 existing tests passed in 17.13 seconds. The new local package is
`build/windows/packages/FreedomUnite-20261007-003116/`, with five overlay DLLs.
All 16 package manifest entries and all 548 preserved baseline files passed
size and SHA-256 verification. A bounded check of the packaged executable
loaded the libraries from its own package directory, reloaded Test, installed
`lobby_task` and reached the same `game_sub` stop at `0x09CFB680`.
Logs are under the same hunter investigation
directory (`lobby-build`, `lobby-strict`, `lobby-tests`, `lobby-package`).
Diagnostic presentation and pacing settings were temporary; the isolated
settings file was restored afterwards. Normal-speed village gameplay and
the hunt loop remain to be checked with fallback disabled.

## Native home control and village entry — 7 October 2026

The Overlays workflow generated `game_sub` into 57 units and linked
`ovl09C19100_game_sub_662DDC5878706505.dll`. Its strict run passed the previous
stop and next stopped at `0x09D63880` in `stage066.ovl`. That small library
was compiled too. Advancing the opening conversation then reached a second
stage, `stage139.ovl`, at `0x09D63900`; it was also preserved and compiled.
No handwritten runtime or framework changes were required.

| Image | Base | Code bytes | Data bytes | Image bytes |
| --- | --- | ---: | ---: | ---: |
| stage066 | 0x09D63100 | 936 | 1,048 | 2,048 |
| stage139 | 0x09D63100 | 776 | 1,336 | 2,176 |

The preserved dumps are `overlays/stage066.bin` and `overlays/stage139.bin`
under the hunter investigation directory. Their SHA-256 values are
`0607D1C5453DA58E2206D10393A8FB081E661FE6491DACD5A8A8E5E5260A9E85`
and `6FDE23C7125EEA06DACE4F49C5BA27BACA5EC4BD34187FE3F7F98657E13B96D0`,
respectively. Runtime traces showed stage066 → stage139 → stage066 while
the opening conversation completed, then stage139 again on exiting the home.

Inspected captures established the rendered introductory conversation,
Test waking and standing in the home, movement from beside the bed to the
door, and the Exit prompt. Scripted Square input exited into Pokke Village;
`captures/village-exit-after.bmp` shows Test outside the home, the village
and the Enter House prompt. The successful runs had the interpreter disabled
and strict HLE enabled, and ended at scripted window closure (exit code 4).
No further missing code was encountered on this path.

All game runs stayed within the 55-second watchdog. Existing diagnostic
unthrottled pacing, immediate presentation and internal scale 1 were used;
the isolated settings file was restored byte-for-byte after each run. This
establishes native entry and movement coverage, not a normal-speed performance
result, shop/quest interactions, or a completed hunt.

All 20 existing tests passed after the final library addition in 17.11 seconds.
The local package is `build/windows/packages/FreedomUnite-20261007-004212/`,
with eight overlay DLLs. All 19 package manifest entries and all 548 preserved
baseline files passed size and SHA-256 verification. A bounded strict check
of that packaged executable loaded its own libraries, completed the same
home-to-village transition and quit through scripted window closure. Its
village capture was inspected. The isolated test save still matches its
backup hashes. Logs, captures and bounded
probe scripts remain local under `build/windows/investigation/hunter/`.

## In-game checkpoint and manual log monitoring — 7 October 2026

The isolated Test hunter completed the bed's Save Game Data confirmation
with strict HLE and the interpreter disabled. AUTOSAVE encrypted 1,483,008
plaintext bytes. The updated save is backed up under
`build/windows/investigation/hunter/saved-after-opening/`; the earlier
character-creation backup remains separate. A cold restart loaded directly
into Pokke Village, without replaying the opening sequence. Village movement,
the main menu, item inventory and NPC dialogue were inspected in subsequent
bounded strict runs. The normal-paced check `normal-village-loaded` reached
the rendered village with vsync and unthrottled mode disabled. Other scripted
interaction checks continued to use temporary diagnostic fast-forward.

The owner then requested manual control while logs were monitored. Manual
sessions were left running for the player's test, overriding the usual
60-second limit for automated runs; no scripted input was supplied. They used
the isolated installation, normal pacing, default internal scale, live
performance and loading traces, and overlay dumping. Logs and local evidence
are under `build/windows/investigation/manual-20261007-010434/`.

The first strict session stopped at `0x09A5F380` in `arcade_task.ovl` (61,716
code bytes, 2,732 data bytes, 64,512-byte image). The dump was preserved and
the workflow generated four units and compiled
`ovl09A5F300_arcade_task_D168A1F992B2349C.dll`. A second manual strict session
installed that library and returned to `lobby_task`, establishing replacement
and reinstallation on this path. Quest entry then stopped at `0x09C10E00` in
`game_task.ovl`: 1,408,724 code bytes, 368,108 data bytes and a 1,776,896-byte
image at `0x09A5F300`.

For the complete manual excursion, fallback was enabled in the third session.
A local watcher preserved each complete MWo3 dump under a name containing
its image name, base and SHA-256 prefix, before a later load could overwrite
the slot-address file. The captured stages all use base `0x09D63100`:

| Image | Code bytes | Data bytes | Image bytes |
| --- | ---: | ---: | ---: |
| stage092 | 504 | 840 | 1,408 |
| stage093 | 448 | 896 | 1,408 |
| stage095 | 448 | 2,304 | 2,816 |
| stage096 | 552 | 664 | 1,280 |
| stage097 | 1,008 | 976 | 2,048 |
| stage098 | 448 | 640 | 1,152 |
| stage099 | 832 | 1,024 | 1,920 |
| stage100 | 552 | 792 | 1,408 |

`overlay-manifest.json` records the ten preserved images, including game_task
and arcade_task, their measured sizes and full hashes. The third session
reinstalled lobby_task and stage139, logged an encrypted save, then ended
with `Runtime stopped: window closed`. No other unimplemented HLE or
unsupported-instruction failure was reported. The log establishes area
traversal, village return, saving and normal closure; quest identity and
completion outcome are not established by these log messages alone.

Most observed gameplay samples were near 30 fps at normal speed. Loading
transitions had brief stalls: one return-loading sample reported 7.5 fps
with a 540.7 ms maximum frame, and load traces placed substantial guest
time in checkSumThread, calcSumThread and sha1Thread. This is evidence for
later loading-time investigation, not a broad performance benchmark.
The interpreter summary counted 142,078,710 instructions across 1,126 unique
entry addresses; the excursion is therefore not a native-only hunt result.

The updated manual save is backed up separately under `saved-after-playtest/`.
All 20 tests passed after adding arcade_task in 17.50 seconds. The local
package is `build/windows/packages/FreedomUnite-20261007-012059/`, containing
nine overlay DLLs. All 20 package manifest entries and all 548 preserved
baseline files passed size and SHA-256 verification. No handwritten runtime
or framework changes were needed.

## Captured quest libraries compiled — 7 October 2026

At the owner's request, game_task and all eight stages captured in the manual
run were compiled while the next manual test was deferred. Each of the nine
input images first matched the size and SHA-256 recorded in the capture
manifest. The existing Overlays workflow generated game_task into 109 units
and the eight small stage corpora, then compiled and linked all nine libraries.
The library count is now 18; game_task is
`ovl09A5F300_game_task_36BD79C16BF70197.dll`. The generated code includes
`0x09C10E00`, the entry that stopped strict quest loading. Its runtime execution
has not yet been verified by the deferred manual test.

All 20 existing tests passed in 17.26 seconds. The package is
`build/windows/packages/FreedomUnite-20261007-014308/`, with 18 overlay DLLs;
all 29 manifest entries passed size and SHA-256 verification. All 548 preserved
baseline files and all four files in the latest manual save still match their
backup hashes.

Both the development and packaged executables passed a bounded headless boot
check with strict HLE, no renderer/audio and the interpreter disabled. Each
accepted 18 available corpora, bound all 272 executable imports with zero
logging stubs, installed demo_sub and demo_task, and stopped at the explicitly
configured one-million-dispatch limit. The expected stop gives exit code 4.
This checks boot and library loading, not native quest execution, rendering,
audio or the complete loop. No manual game session was left running.

The local `build/windows/Playtest-Native.cmd` and companion PowerShell helper
start the development executable with strict HLE and fallback disabled, using
the isolated Test save and normal settings. They provide no scripted input,
hold the launch session open until the player quits, and put logs and missing
overlay dumps in a new manual-native directory per run. The helper passed
syntax checks in both the current shell and Windows PowerShell; its complete
interactive run is deferred to the owner. It is not part of the package.
Build/check logs are the `quest-build`, `quest-tests`, `quest-package`,
`quest-headless` and `quest-package-headless` files in the manual investigation
directory. No handwritten runtime or framework changes were needed.

## Slay Giaprey strict playtest — 7 October 2026

The owner reported buying and equipping armor and a weapon, entering Slay
Giaprey and killing several enemies before the game stopped. The run in
`build/windows/investigation/manual-native-20261007-033428/` had the
interpreter disabled. Its log records successful installation of game_task
and stage098, stage099, stage092, stage093, stage100, stage097, stage096 and
stage095, then replacement of stage095 by the previously uncaptured
`stage094.ovl`. The explicit stop was
`No recompiled function registered at 0x09D63680`. The evidence identifies
a missing stage corpus; it does not identify a combat or equipment defect.

The missing image was preserved as `overlays/stage094.bin` in that run's
directory. Its declared code is 544 bytes and data 928 bytes, with a 64-byte
header. Full-image generation includes the stopping address, which lies in
the declared data region. The compiled library is
`ovl09D63100_stage094_66742CE05AECE8D4.dll`, bringing the total to 19.
No handwritten runtime or framework change was needed. The current four-file
test save was backed up under `saved-after-playtest/` with a separate manifest;
the run logs record loading saves but no save write during this session.

All 20 tests passed in 18.99 seconds. The updated package is
`build/windows/packages/FreedomUnite-20261007-034544/`. Development and package
headless boot checks accept 19 corpora, bind imports with zero logging stubs,
and stop at the configured one-million-dispatch limit with fallback disabled.
The first boot check used the wrong dispatch-limit variable and was killed
by its 20-second watchdog; it was repeated using PSPRECOMP_MAX_DISPATCHES.
This verifies boot, not execution of stage094 or quest completion. The next
manual retry should use the same `build/windows/Playtest-Native.cmd` launcher.
All 30 package manifest entries and all 548 preserved prototype files passed
size and SHA-256 verification after this change.

## Slay Giaprey completed without fallback — 7 October 2026

The owner reports successful completion of Slay Giaprey after the stage094
addition. The corresponding run is
`build/windows/investigation/manual-native-20261007-034816/`, launched through
the strict native playtest helper. The log records installation and execution
of stage094, followed by game_task being replaced by lobby_task and stage094
being replaced by village stage139. Save logs record two successful encrypted
save writes. The terminal reason is `Runtime stopped: window closed`; no
missing-corpus, unsupported-instruction or unimplemented-HLE stop is recorded.
Quest success is established by the owner's observation; the logs corroborate
native execution, village return, saving and normal closure.

The four current save files were copied to `saved-after-quest/` in that run's
directory and verified against a separate SHA-256 manifest. No build or source
change was needed for this successful retry. This verifies one complete hunt
and village return with fallback disabled, not every quest or the full campaign.
A cold restart of this completed quest save remains to be checked.

## Offline library inventory — 7 October 2026

At the owner's request, scripts/scan-overlays.py scanned the complete local
886,702,080-byte ISO and read the supported executable's named overlay sections.
The raw ISO scan found zero MWo3 headers, including none for the 19 libraries
already captured successfully at runtime. Consequently this method cannot
extract the archive images or validate their image hashes. ISO directory
inspection identifies a 772,732,928-byte PSP_GAME/USRDIR/DATA.BIN archive;
its internal encoding was not established by this scan.

The executable declares 299 overlay sections at the five measured slot bases:
12 tasks, two subtasks, 17 monster libraries, 267 stages and one debug. Matching
the declarations by name and base to existing DLLs finds all 19 compiled
libraries: six tasks, both subtasks and 11 stages. The 280 unmatched declarations
are six tasks, 17 monster libraries, 256 stages and debug. The task names are
movie_task, install_task, show_select_task, download_task, gallery_task and
option_task. A declaration does not prove a library is used or mandatory;
uncaptured image hashes and runtime execution remain unverified.

The full metadata inventory and missing names are saved locally in
`build/windows/investigation/overlay-inventory/disc-overlays.json` and REPORT.md.
No game images were extracted and no build, save or prototype file was changed.
Offline archive extraction/decompression is the next investigation for expanding
coverage without requiring the owner to progress through the story.

## Offline archive extraction succeeded — 7 October 2026

The standalone [mhef](https://github.com/svanheulen/mhef) DataCipher tool was
used externally with its USA game selector to decode DATA.BIN entries from
the read-only disc image. Its source, GPL license and Python compatibility
adaptation are retained only in ignored research storage; no external GPL
implementation was copied or translated into the port or packaged runtime.
[mhff](https://github.com/svanheulen/mhff) was consulted for archive format
facts, not executed. Tool revisions are recorded locally.

The archive table contains 6,657 entries and passed count, monotonic sector
offset and bounds validation. Scanning decrypted entry headers recovered
298 unique MWo3 images: 12 tasks, two subtasks, 17 monster libraries and
267 stages, totalling 6,287,360 bytes. Each extracted image passed header,
name/base, size, bounds and SHA-256 verification. The header/code fingerprints
of all 19 already compiled libraries match the extracted originals exactly.
This makes the other 279 images available without gameplay progression;
none of those 279 has been compiled or runtime-tested by this extraction step.
Only debug.elf, the remaining executable declaration, was not recovered.

The local runner, extraction log, full image manifest, verification record and
report are under `build/windows/investigation/archive-research/`; images are
in its `extracted-overlays/` directory. All 548 preserved prototype files and
all four completed-quest save files still match their backup hashes. The
playtest build and package remain at 19 compiled libraries.

## All recovered game libraries compiled — 7 October 2026

The remaining 279 images passed their extraction-manifest size and SHA-256
checks before generation. The existing Overlays workflow compiled them in ten
sequential batches at two jobs: eight batches of 32 stages, 17 monster libraries
and six task libraries. No handwritten runtime or framework change was needed.
The resulting build contains all 298 recovered game overlays: 12 tasks,
two subtasks, 17 monster libraries and 267 stages. debug.elf remains unrecovered.

A separate metadata verification loaded all 298 DLLs and checked ABI version 2,
base, image size, code size, identity fingerprint, name and the registration
export against their extracted inputs. All passed. All 20 regression tests
passed in 18.09 seconds. The final separate package is
`build/windows/packages/FreedomUnite-20261007-044353/`; all 309 manifest entries
passed size and SHA-256 verification and all 298 packaged DLLs match the
metadata-verified development copies.

Development and final-package executables each passed headless strict boot
with the interpreter disabled, no renderer/audio, a one-million-dispatch limit
and an external 55-second watchdog. Both accepted 298 corpora, bound 272 imports
with zero logging stubs, installed the boot overlays and stopped at the expected
dispatch limit. No library loading or ABI rejection was reported. This validates
loading and boot, not execution of every overlay, rendering or a full campaign.

All 30 entries of the previous 19-library playable package, all 548 prototype
baseline files and all four completed-quest save files remain hash-verified
and unchanged. The local native playtest launcher uses the expanded development
build automatically. Logs and verification manifests are under
`build/windows/investigation/archive-research/`: compile-01 through compile-10,
full-tests, full-package, full-headless, full-package-headless,
compiled-verification-windows.json and full-preservation-verification.json.

## Startup stall with the expanded library set — 8 October 2026

The owner reported that launching opened another window without reaching the
game, then printed an exit log when that window was closed. Both logs from
`manual-native-20261008-214950/` were empty. No matching Windows application
crash event was found. A headless reproduction remained alive with no output
and was terminated by its 55-second watchdog; that termination was a diagnostic
timeout, not a guest crash. Previously reported boot checks followed bulk DLL
validation and did not catch this later launch stall.

The framework's overlay loader previously opened every DLL at startup, before
its first progress line. It now indexes canonical generated base/hash filenames
and opens a DLL only when its header/code identity matches a guest overlay.
The actual module ABI and exported metadata remain checked by the existing
loader, and the corpus identity remains checked before installation. Older
noncanonical names retain eager loading; MHFU_OVERLAY_EAGER_LOAD=1 restores the
previous behavior for comparison. Loading progress is flushed before each
requested DLL is opened. The complete framework patch includes the change.

The game target, backend-independent portablekit_host_check and I/O filter
test target rebuilt successfully. All 20 tests passed in 27.54 seconds.
Strict headless boot indexed 298 libraries, loaded and installed only demo_sub
and demo_task, then reached its expected one-million-dispatch stop in 5.08
seconds. A rendered strict run with audio enabled reached the language-selection
screen, which was captured and visually inspected. Its scripted close completed
in 22.39 seconds within a 55-second watchdog, with no game input or save write.
Exit code 4 accompanied the expected dispatch-limit/window-closed reasons.

The new separate package is
`build/windows/packages/FreedomUnite-20261008-220048/`. Its strict headless
boot also indexed 298 libraries and installed both boot corpora within a
20-second watchdog. All 309 package manifest entries passed verification.
The earlier 298-library and 19-library packages, all 548 preserved prototype
files and all four completed-quest save files remain hash-verified and unchanged.
The native playtest helper now specifies the executable's working directory
and records exit code, hexadecimal code, elapsed time and terminal reason in
exit.json. Syntax checks pass in both current and Windows PowerShell shells.
The complete patch also passed application and reverse checks in an isolated
repository containing pristine pinned framework sources; its reapplied loader
source matches the working source exactly after line-ending normalization.

Evidence is under `build/windows/investigation/boot-failure-20261008/`:
original and fixed headless logs, fixed-render logs and captures, build/tests/
package logs, packaged boot logs and preservation.json. A full manual hunt,
overlay replacement and cold reload after this loader change remain unverified.

## Expanded native quest loop verified — 8 October 2026

The owner reported that the fixed build loaded the save, completed a quest,
saved and returned to the village successfully. The corresponding strict manual
run is `build/windows/investigation/manual-native-20261008-220225/`. The log
records 14 DLLs loaded on demand, including cont_task, lobby_task, game_sub,
game_task and the visited stage libraries. Previously loaded stages are
installed again without reopening their DLLs. The return replaces game_task
with the cached lobby_task corpus and the quest stage with cached stage139.
Save logs show a successful encrypted write; the terminal reason is
`Runtime stopped: window closed`. No missing-library, unsupported-instruction
or unimplemented-HLE stop was recorded. Quest completion is established by the
owner's observation; the log corroborates the library transitions and saving.
The quest identity was not specified by the owner or established by the log.

This verifies the observed hunt loop and corpus replacement/reuse after the
on-demand loader change. It also verifies loading the previously completed
quest save. It does not establish execution of all 298 libraries or a cold
reload of the newly written save. All four current save files were backed up
under `saved-after-quest/` in the run directory and verified against a separate
SHA-256 manifest. No game source or compiled binary change was needed.

The launcher diagnostic JSON recorded a null exit code and incorrectly formatted
it as hexadecimal zero; the runtime log, rather than that hexadecimal field,
establishes normal window closure. The helper now retains the process handle
before waiting, and leaves an unavailable hexadecimal code null. This is a
diagnostic fix only; it does not alter the game. The original run record remains
preserved as evidence.

## Next implementation step

Broaden gameplay with fallback disabled as progression permits, recording
different quests, monsters, maps and equipment paths. Check a cold reload of
the latest completed-quest save when the owner next launches the game. The
library images no longer require story progression to obtain; actual gameplay
verification still does. Compilation alone does not establish runtime
correctness. Keep fallback available for ordinary play during coverage work.
