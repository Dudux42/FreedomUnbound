# Texture composition and launcher management

9 October 2026. This foundation manages compatible artwork packs. No HD artwork
or controller sheets have been authored/bundled. Camera/L targeting, QoL, and
native 60 FPS remain separate work.

## Launcher

Choose **Mods** on Home to turn texture replacements on/off, import a checked
pack, open its folder, or **Use original artwork**. Preferences use the existing
settings file, not game saves. Turning replacements off retains the pack.

```powershell
./scripts/playtest-native.ps1 -Launcher
```

The local `build/windows/Launcher.ps1` starts the same logged native-only launcher
with isolated Test data. The usual playtest still boots the game directly.

## Compose our HD and controller artwork

Layers are ordered lowest priority first:

```powershell
python scripts/compose-texture-packs.py `
  --layer 'art/hd-base' `
  --layer 'art/hud-xbox' `
  --output 'build/combined-xbox-pack'
```

Import the output folder through Mods. It contains `textures.ini`, winning PNGs
under `images/`, and `composition.json` recording layer ownership/overrides.
Composition does not select or enable the result. Source packs remain unchanged;
existing output directories are refused. Use a new folder for each composition.

Later layers override matching keys. An empty mapping restores original artwork
for that key. Identical winning images are deduplicated by SHA-256. The winning
layer chooses its filter; omitted filters use Auto. All inputs are validated
before staged output assembly; sources are not overwritten.

## Supported initial format

Flat INIs with `[options]`, `[hashes]`, and optional `[filtering]`; explicit
24-digit hexadecimal base-level keys; PNG images; hash xxh32/xxh64; compatible
normalized hashing options across every layer. Auto/nearest/linear filters are
supported. Escaping paths, unsupported options, incompatible hashing, invalid
keys, and structurally damaged PNGs are rejected.

Game-specific INI overrides, hash/reduction ranges, mip overrides, archives,
implicit hash-named mappings, and KTX2 are not supported. Flatten explicit
mappings first. This tool targets our own controlled bundles, not arbitrary
community-pack conversion. PNG structure/checksums are verified; visual output,
pixel decoding, and GPU residency still need renderer checks. File validity does
not establish artwork ownership or permission to distribute it.

## Verification

Six synthetic-art tests cover priority, original fallback, deterministic output,
deduplication, filtering, normalized booleans, incompatible hashes, invalid paths/
keys/PNGs, protected output paths, and source preservation. They run through CTest
as `mhfu_texture_composition_tests`.

Inspected native launcher captures verify Mods navigation and toggle persistence.
A composed two-key synthetic pack passed the native import review with both PNGs
reported. This establishes format integration, not actual HD gameplay coverage.
