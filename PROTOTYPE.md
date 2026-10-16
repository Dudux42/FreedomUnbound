# Preserved working prototype

Baseline: `baselines/2026-10-06-working-prototype/` (local only, excluded from Git).

The owner personally verified loading an existing hunter, moving around the village, starting and completing a hunt, saving, returning to the village, quitting, reopening the application, going on another hunt, and returning to the village. This verifies the core gameplay loop. It does not establish full campaign coverage or character creation in that session.

The exact executable used in that session was not identified. Four different executable versions were found and preserved rather than assuming they were identical:

| Baseline folder | Original location |
| --- | --- |
| `applications/packaged-release/` | `out-release/package/FreedomUnite/` |
| `applications/release-bin/` | `out-release/bin/` |
| `applications/render-bin/` | `out-render/bin/` |
| `applications/played-recompiled/` | `out-render/played-20261005-recompiled/` |

Run `FreedomUniteNative.exe` with its adjacent DLLs from the appropriate application folder. The application still needs the player's own installed game data or supported USA ISO (`ULUS10391`). Preserve this baseline independently of subsequent builds.

The baseline also holds the current source tree (including the modified PortableKit framework), generated C++, build configuration records, logs and existing captures. `manifest.json` records each copied file's relative source path, size and SHA-256. `verification.json` records the verification result. Current source is a snapshot for investigation; it is not proven to be the exact source that produced each preserved executable.

No disc images, extracted game executables, saves or user settings were copied into this baseline. Existing user game data remains in place. The baseline is a local copy on the same drive, not protection against drive failure.

Interpreter fallback remains enabled for normal play. The previously audited release-bin executable stopped at missing address `0x089B93D8` with fallback disabled; resolving that coverage is a later step.

Next step: document and verify a reproducible build, using a separate build directory so these applications remain intact.
