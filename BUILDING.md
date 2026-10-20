# Building Freedom Unite on Windows

This workflow builds from source into `build/windows/`. It never reuses or overwrites `out/`, `out-render/`, `out-release/` or `baselines/`. Run commands from the repository root in PowerShell. The script serializes its own stages across the checkout because build directories share generated code. Do not run a separate CMake build concurrently with code generation.

## Prerequisites

- Windows x64 and Visual Studio 2022 C++ Build Tools with the Windows SDK. The script discovers VS 2022 with `vswhere`, or uses an existing developer shell.
- Git, CMake 3.20 or newer, Ninja and Python 3 on PATH.
- vcpkg with SDL3 (Vulkan feature), Vulkan headers and loader, and `glslangValidator`. Set `VCPKG_ROOT`, or pass `-VcpkgRoot` to the script. If the shader compiler is elsewhere, pass `-Glslang` with its full path.
- Optional `ccache` on PATH. A fresh game-code compile can take hours; cache hits can make subsequent builds much faster.
- Your supported USA ISO (`ULUS10391`) for installation and code generation.

With an existing vcpkg checkout, the package names/features are:

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install 'sdl3[vulkan]:x64-windows' 'vulkan:x64-windows' 'glslang[tools]:x64-windows'
```

The local toolchain used for validation is MSVC 19.44, SDL 3.4.16, Vulkan headers/loader 1.4.357.0 and glslang 16.4.0. These describe the tested environment; the script does not pin or install vcpkg packages. Record the versions in a different environment before comparing results.

FFmpeg is pinned by PortableKit to its Windows x64 shared 7.1.5 build and checked by SHA-256. Configuration downloads it if it is not cached. `-DownloadCache <directory>` can point to a prepopulated download cache for an offline build. No audio or movie support is silently disabled by this workflow.

## Initialize the framework

```powershell
git submodule update --init --recursive
./scripts/build-windows.ps1 -Step Patch
```

The submodule revision is `8b0c3fedd1d908dc2d171f8257c5c3d9af8886aa`. The saved `patches/portablekit-local.patch` is required: it supplies profile fields, controls, save/message dialogs, the launcher, CPU skinning and sparse indexed geometry decoding, and their regression tests. Patch applies it only to a clean checkout at that revision. If already present, the script checks it and proceeds. Conflicting local changes stop the workflow rather than being reset.

The build itself also reports a missing patch instead of failing later on profile fields. When intentionally changing the framework, update the saved patch before using this workflow again.

## Build in stages

Set your dependency inputs once. These are examples, not paths built into the repository:

```powershell
$dependencies = @{ VcpkgRoot = $env:VCPKG_ROOT }
# If needed:
# $dependencies.Glslang = 'C:/path/to/glslangValidator.exe'
# $dependencies.DownloadCache = 'C:/path/to/ffmpeg-download-cache'
```

1. Build the installer and recompiler without compiling any existing generated corpus:

   ```powershell
   ./scripts/build-windows.ps1 -Step Bootstrap @dependencies
   ```

2. Install from your own disc image. This verifies the disc ID and executable hash, reads the plain `BOOT.BIN`, and writes `build/windows/game/EBOOT.ELF`. `--in-place` retains the original ISO where it is. Saves and settings from your normal installation are not imported.

   ```powershell
   ./scripts/build-windows.ps1 -Step Install -Iso 'C:/path/to/your-game.iso'
   ```

3. Generate the corpus. The script checks the installed executable against the profile hash first. Generation writes `generated/` in the checkout; it may update that local corpus, but never the preserved baseline.

   ```powershell
   ./scripts/build-windows.ps1 -Step Generate
   ```

4. Compile the native application and the existing test targets:

   ```powershell
   ./scripts/build-windows.ps1 -Step Build @dependencies
   ```

   The build uses Release, generated optimization level 2, two compile jobs by default, bundled FFmpeg, graphics enabled and distribution mode. `-Jobs 1` can reduce memory use. Missing graphics dependencies are an error; a headless build must be requested explicitly outside this workflow.

5. Run tests, then assemble a new package with adjacent DLLs and file hashes:

   ```powershell
   ./scripts/build-windows.ps1 -Step Test
   ./scripts/build-windows.ps1 -Step Package
   ```

   Tests use temporary folders and loopback networking. A restricted sandbox can prevent those operations; distinguish environmental failures from test defects. The tests exercise the framework, not the full gameplay loop or all new system-dialog behavior.

Packages have timestamped folders under `build/windows/packages/`. Package does not rebuild or run tests; use the stages in order. It copies only the named application files, so test executables, installed game data and saves are left out. The package includes source/build identifiers, file hashes and launch instructions. MSVC builds use the shared Visual C++ x64 runtime; that runtime and a working Vulkan graphics driver are machine prerequisites.

For an external-playtester candidate, use `-Step Package -AlphaCandidate` after
a standard Build and Test. This rejects native 60 FPS research/action diagnostic
builds and adds a dedicated launcher executable, fallback shortcut, timestamped
session logs and tester instructions.

For the complete sequence in one invocation:

```powershell
./scripts/build-windows.ps1 -Step All -Iso 'C:/path/to/your-game.iso' @dependencies
```

For source-only edits after initial preparation, repeat Build, Test and Package. Do not regenerate unchanged game code unnecessarily. Generated files and all build/installation outputs remain ignored by Git.

The Test stage isolates `MHFU_` and `PSPRECOMP_` environment switches while tests
run, then restores the caller's values. This prevents strict manual-playtest
settings from disabling interpreter coverage in the runtime tests. Build metadata
now hashes all local host and workflow sources. Package refuses changed sources
as well as changed executables/overlay libraries; rebuild before packaging.

For the optional 30 FPS action-timing research build, add
`-ActionTimingDiagnostics` to Build. This compiles observed copies of two
generated units under the build folder, leaves originals unchanged, and records
the diagnostic option in `build-info.json`. A normal Build without that switch
restores the original units. This switch does not enable native 60 FPS or mods.

Run the read-only local dependency/provenance audit after building:

```powershell
python scripts/diagnose-project.py --output build/windows/project-audit.json
```

Add `--package <package-folder>` to verify a package's recorded hashes. The
auditor checks PE imports, executable identity, metadata/source freshness,
overlay count, package hashes, and excluded artifacts in the tracked root
repository. It does not execute binaries, inspect save contents, prove driver
compatibility on another machine, or establish gameplay correctness.

Generate also includes `0x089B93D8–0x089BAB70`, the measured startup constructor
block embedded in this executable's `.data` section. Its exclusive end is the
constructor-pointer table, which is not compiled as code. The stage writes
`generated/mhfu-codegen.json` with the executable hash, extra ranges and registry
hash. Build rejects missing or stale generation metadata; rerun Generate when
upgrading from the earlier corpus.

## Generate and build local overlay libraries

The main ELF does not contain the runtime-loaded overlay images. The boot
libraries were generated from local `MWo3` dumps of `demo_task.ovl` and
`demo_sub.ovl`; `edit_task.ovl` was subsequently added for character creation.
To add your own captured images, pass their paths to the
Overlays stage, using the same dependencies as Build:

```powershell
./scripts/build-windows.ps1 -Step Overlays -OverlayDump @(
    'dump/demo_task.bin',
    'dump/demo_sub.bin'
) @dependencies
```

The stage checks the supported installed executable and each dump's header,
length and guest-memory bounds. It reads the load address from the header,
generates into the ignored root `overlays/` tree, then builds the native
application and overlay targets. The existing framework wrapper also emits
data-region entry points, which are needed for this game's embedded overlay
constructors. A cold checkout needs local dumps; All does not extract them
from the ISO automatically. All accepts `-OverlayDump` if dumps already exist.

Set `MHFU_DUMP_OVERLAYS` to an ignored local directory to capture unknown
images during a bounded gameplay run with fallback enabled. Existing dumps
are named by slot address and can be overwritten when an image is replaced;
preserve each image before advancing.

Build records the expected overlay DLL names and hashes. Package checks them,
copies them into an `overlays/` subfolder and includes their relative paths in
its recursive hash manifest. Preserve that subfolder when sharing or moving
the package.

Libraries with the standard generated base/hash filenames are indexed without
opening all DLLs at startup. Each DLL loads on the first matching guest overlay;
its ABI and exported metadata are checked before its corpus is installed.
This avoids the observed startup stall with 298 libraries. Libraries using
older filenames keep the existing eager-loading behavior. For comparison,
`MHFU_OVERLAY_EAGER_LOAD=1` restores eager loading for all libraries.

## Run the new build

```powershell
$env:MHFU_DATA_DIR = (Resolve-Path 'build/windows/game').Path
./build/windows/bin/FreedomUniteNative.exe
```

This uses the isolated installation and its own saves. Unset `MHFU_DATA_DIR` before launching your usual installation. Share the entire package folder, including DLLs; testers use their own supported ISO.

Keep interpreter fallback enabled for ordinary play during coverage testing.
`PSPRECOMP_NO_INTERPRETER=1` has been checked through
creating and saving an isolated hunter, restarting, reloading it, completing
the opening dialogue, moving inside the home and exiting into Pokke Village.
The initial village-entry checks used temporary diagnostic fast-forward
settings. In-game saving and cold reload into the village have since passed
with normal-paced reload. Manual strict testing also passed loading and
unloading `arcade_task`. The `game_task` library that stopped quest entry and
all eight captured stage libraries are now compiled and packaged. Development
and packaged builds passed a bounded headless boot check with fallback disabled.
A subsequent manual run with stage094 compiled completed Slay Giaprey,
returned to the village, saved and closed normally with fallback disabled.
Broader quests and maps still need runtime testing.

The executable inventory declares 298 game overlay libraries plus debug.elf.
All 298 game images have been extracted locally from the supported disc;
the separate debug declaration was not recovered. Extraction metadata and
images stay under the ignored investigation directory. The standalone archive
research tools are not part of the port or its package. Use the existing
`Overlays` step with batches of validated extracted images and two jobs to
expand coverage; a compiled library is not evidence that its gameplay path
has passed a manual test.

For the current local installation, double-click
`build/windows/Playtest-Native.cmd` to run the new build against the isolated
Test hunter. This local helper disables fallback, supplies no scripted input,
and records logs and any missing overlay under a fresh
`build/windows/investigation/manual-native-<timestamp>/` directory. Close the
game normally after testing. The launcher is local to this installation and is
not included in the distributable package.

## Versioned alpha packaging

After building the standard configuration and passing `-Step Test`, package the
first release with:

```powershell
./scripts/build-windows.ps1 -Step Package -AlphaCandidate -ReleaseVersion 0.3.0-alpha.2
```

This creates `build/windows/packages/FreedomUnbound-0.3.0-alpha.2-windows-x64`.
The version option requires alpha packaging, includes `RELEASE_NOTES.md`, and
refuses to overwrite an existing package. Archive the entire folder and validate
its manifest and extracted startup before uploading. Keep the preserved prototype
and previous candidate packages intact.

Windows builds also compile `FreedomUnbound.exe`, a GUI entry point built with
the static MSVC runtime. Alpha packages include it alongside the game executable
and logging script; the build record verifies its hash before packaging. Run it
from the extracted package, not directly from `bin` without the startup script.
Its optional `-DataDirectory`, `-LogDirectory` and `-SmokeTest` switches support
isolated startup checks. Smoke testing requires explicit data and log paths.

## Validation on 6 October 2026

- The saved PortableKit patch applied to a pristine archive of the pinned upstream revision, and passed a reverse-application check afterwards.
- Bootstrap compiled in a new build directory. Installation from the supported ISO produced the expected executable hash in the isolated installation.
- Code generation reproduced all 67 units and the registry/header without changing their contents from the preserved baseline.
- The complete native application, host check and test targets compiled. Subsequent incremental builds required no source recompilation.
- CTest discovered all 20 tests from the root build directory. The confirmation run passed all 20. The initial run had one texture-pack directory-swap failure; a targeted rerun and then the full confirmation run passed. Its transient cause is not established; both logs are retained locally.
- A bounded rendered boot reached language selection on the local RTX 3060, opened 44.1 kHz stereo audio, bound all 272 main-executable imports with no logging stubs, and quit through scripted window closure. This runtime reports exit code 4 for that deliberate stop. The capture was inspected. Interpreter fallback was exercised, as expected.
- Graphics configuration was rejected both with the renderer explicitly off and with it requested but SDL discovery disabled. The workflow also rejected using the original release directory as a build target.
- The preserved baseline's 548 file hashes remained intact.

This validates the local build workflow and boot, not the new executable's complete gameplay loop, another machine, or byte-identical builds across different toolchains. Runtime fixes and interpreter coverage work are separate next steps.

## Named test package

After the standard Build and Test steps, use:

```powershell
./scripts/build-windows.ps1 -Step Package -AlphaCandidate -PackageFolderName FreedomUnbound-test
```

This creates `build/windows/packages/FreedomUnbound-test` and refuses to overwrite
an existing folder. The package includes `TEST_BUILD_CHANGES.md` and a matching
summary in `README.txt`; the root README links to the full change log. This
package excludes the standalone save editor, personal game data and preview
caches. Publish the ZIP as a private GitHub release asset; binaries are not source
files and remain outside Git history. Preserve earlier packages.
