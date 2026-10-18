# Launcher mod catalogue

16 October 2026. Windows implementation.

## Players

Open **Mods > Browse downloadable mods**. The built-in
[FreedomUnboundmods catalogue](https://github.com/Dudux42/FreedomUnboundmods)
is always listed. Click an index heading to expand its mods; click again to
collapse it. Each expanded index shows mod names, versions, download sizes and
installed versions.
Choose a mod to download it, then review its author, type, description and any
save-impact or replacement warning before pressing **Install**. New mods remain
disabled until switched on in the installed list. Replacements retain the
existing enabled state through the normal installer.

The matching installed version is marked Installed and cannot be downloaded
again from the catalogue. A different published version can be selected and
reviewed; versions are compared for equality, not assumed to be upgrades.
Use **Refresh this index** inside an expanded group to see repository changes. ZIP drag-and-drop installation
continues to work. Move up/down controls remain removed.

Downloads run off the UI thread. Progress and cancellation are available; Back
also cancels an active transfer. Cancellation finishes after an outstanding
network operation returns or times out. Downloaded ZIPs stay in
`<data>/mod-downloads/`; cancelling or failing removes partial files. An intact
cached ZIP can be reused after checking its size and SHA-256 again. No mod is
installed automatically. Personal saves are not part of this process.

The last valid index is cached for browsing if a refresh fails. The screen shows
that it is cached and explains the connection error. A cached verified ZIP can
still be reviewed offline. Nothing falls back to an unchecked download.

## GitHub index contract

Source: `https://raw.githubusercontent.com/Dudux42/FreedomUnboundmods/main/releases.json`.
The existing index format is used directly:

```json
{
  "release": "mods-YYYY-MM-DD",
  "mods": [
    {
      "id": "example",
      "name": "Example",
      "version": "1.0.0",
      "file": "example-1.0.0.zip",
      "bytes": 1234,
      "sha256": "<64 lowercase hexadecimal characters>",
      "url": "https://github.com/Dudux42/FreedomUnboundmods/releases/download/mods-YYYY-MM-DD/example-1.0.0.zip",
      "guide": "guides/example.md"
    }
  ]
}
```

Publish the ZIP first, measure its exact byte count and SHA-256, and then update
`releases.json`. Its ID and version must match the ZIP's `mod.json`. Keep both
the repository and release assets public for anonymous launcher access. No
GitHub token, developer credential or account login is embedded or required.
Do not include original game data in a mod-index source commit.

The client accepts a maximum 2 MiB index, 512 unique entries and 2 GiB per ZIP.
ZIP URLs must match the selected index’s GitHub repository, release tag and filename exactly. HTTPS
redirects are bounded and restricted to GitHub's download hosts. ZIP transfer
and checksum verification use fixed-size buffers rather than loading a large
texture pack into RAM. A size/hash mismatch is refused. Existing archive-path,
manifest, API, native-provider and game compatibility checks still run before
installation. The catalogue does not add native providers to an older executable.

## Code and validation

- `host/mod_catalog.*`: bounded JSON parsing, HTTPS transfers, caching, checksums.
- `host/mods_page.cpp`: catalogue, progress, review and installed state.
- `tests/mod_catalog_tests.cpp`: malformed/duplicate data, Unicode, origin and
  path restrictions, size/hash mismatch, cached reuse and cancellation.
- The test executable also supports `--live <cache directory> <mod ID>` for an
  explicit public-index/download check; normal CTest does not require internet.

Ignored evidence is under `build/windows/investigation/launcher-refresh/catalog-*`.
The original small-package live checks fetched all 12 index entries and verified
all 11 small ZIP assets. Native UI checks installed Damage Numbers into a copied
profile, retained its disabled state, displayed Installed in the catalogue and
left copied-save hashes unchanged. Cached-index fallback was inspected while
the repository was still private. Cancelling the HD transfer during startup
returned to the catalogue and left no partial ZIP; the full HD archive was not
downloaded for this check. English and Portuguese catalogue captures were
inspected. All four relevant CTest targets passed on the final build, including
a checksum fixture crossing multiple read buffers. A fresh small ZIP download
and verified cached reuse also passed with streamed hashing. All native runs
retained identical copied-save hashes.


## Additional indexes — 17 October 2026

Choose **Add mod index** and enter a public GitHub repository URL or the full
HTTPS address of its index JSON file. A repository URL assumes
`main/releases.json`; use the raw JSON or GitHub blob URL for a different branch
or filename. Indexes use the schema above, with ZIP assets published in their
own repository. The default publisher is not required for custom indexes.

Custom indexes are saved in `<data>/mod-indexes.txt` and reload on startup. The
built-in index stays first and cannot be removed. Expanded custom groups expose
**Remove this index**, which removes only the saved address; installed mods and
cached downloads remain. Repository URLs, raw URLs and equivalent blob URLs are
normalized, and duplicate addresses are refused. Up to 32 indexes, including
the built-in one, are supported. Index groups start collapsed; the catalogue is
fetched when first expanded and can then be refreshed independently.

Each custom index gets its own cache under
`<data>/mod-downloads/indexes/<URL hash>/`, preventing same-named ZIPs or cached
indexes from different sources from colliding. The built-in index keeps its
previous cache location. Entries are validated against the repository belonging
to their index, including cached data. Mod IDs remain global installation IDs:
a matching installed ID/version is marked Installed in every index listing it.

Validation covered URL normalization/rejection, registry round trips, duplicate
refusal, malformed saved entries, origin restrictions and cache isolation.
The four relevant CTest targets passed. Bounded native runs added an alternate
public URL for the existing catalogue, confirmed persistence after restart,
reviewed a downloaded Gathering Nodes ZIP from its isolated cache, collapsed
both sections and removed the test index. The built-in index remained; copied
save hashes were unchanged. This test did not create another GitHub repository.
Evidence: ignored `build/windows/investigation/launcher-refresh/catalog-groups/`,
`catalog-add-index/` and `catalog-index-restart/`.
