# Launcher save preview

The home screen reads a timestamp-checked summary of the installed USA
`ULUS10391/MHP2NDG.BIN` save, without starting the game or modifying save files.
It displays three taller character cards with cached 3D previews, hunter name,
Hunter Rank, Zenny, Pokke Points, Guild Points, equipped gear, and the time recorded at the last save. The game produces
the summary when it loads or saves; the standalone editor uses the same codec.
Empty slots are shown separately from an unavailable preview. Preview failure
does not disable Play or replace the game's own save validation.

PSP encryption and SFO/file integrity are handled by PortableKit's existing
save loader. The per-game reader independently implements the game's inner
encoding and verifies its salted SHA-1 checksum before extracting fields.
Other editions and unknown file sizes are not guessed. File sizes are bounded
before loading; the decoded result is cached, with file timestamps checked
once per second and refreshed on returning to the launcher home screen.

## Measured USA format facts

Offsets below refer to the fully decoded save, with each slot adding
`0x6B100` to these offsets:

| Field | Offset | Representation |
| --- | --- | --- |
| Name | `0x1000` | Eight UTF-16LE code units |
| Character marker | `0x1012` | Zero: empty; one/two: occupied |
| Equipped gear | `0x1020` | Six 12-byte records: weapon, feet, helmet, torso, braces, waist; presence byte, type byte, little-endian 16-bit ID |
| Play time | `0x4FC8` | Unsigned little-endian seconds |
| Hunter Rank | `0x501C` | Byte, zero-based (display 1–9) |
| Pokke Points | `0x6A248` | Unsigned little-endian integer |
| Guild Points | `0x6A24C` | Unsigned little-endian integer |
| Zenny | `0x6A250` | Unsigned little-endian integer |

These facts were checked against this project's supported USA executable.
The character selector at guest `0x088C0920` reads the name, marker, time,
money, and three-slot stride. The game's rank display at `0x0887CED8` reads
the character-state byte at offset `0x44BC` and adds one. Character-state
offsets map to saved offsets by adding `0xB60`; the time field independently
confirms this mapping (`0x4468` → `0x4FC8`). At `0x088541F8`, the game copies
the same rank into its own Guild Card.

Inner decoding facts were measured at `0x088C2D74`–`0x088C2FD0`.
The 256-byte substitution table is at `0x089B2DB4`; the checksum salt is at
`0x089B4F18`. The checksum routine at `0x088C022C` confirms the signed extent
and stored digest offsets. The PSP save key was observed in the game's
savedata request. No game executable, decoded save, or generated code is
included in the repository.

Public research was used only as a format reference:
[mhef](https://github.com/svanheulen/mhef),
[MHP2G save patterns](https://github.com/IncognitoMan/mhp2g-patterns/blob/main/patterns/mhp2g/savedata.hexpat),
and [MHFU editor offsets](https://github.com/Algorytmo/MHFU-Saveditor/blob/main/source%20code/offsets.py).
No third-party implementation was copied or translated. The reader and
synthetic test fixture generator are independently written for this project.

The synthetic tests cover all three slots, both occupied markers, empty
slots, UTF-16 surrogate pairs, rank limits, time formatting, seed defaults,
corruption, truncation, PSP encryption round trips, oversized files, and
unchanged save-file hashes after previewing.

## Equipped-character portraits (15 October 2026)

The local launcher now places a static portrait above each occupied hunter's
existing information. The logo area is shorter to make room for taller cards;
the compact layout keeps Play above them. Native captures were inspected at
960x544 and 480x272. Clicking a card does not auto-load a hunter in this phase.

`host/save_portraits.cpp` observes the existing once-per-flip profile camera
callback, delegating all camera behavior unchanged. The supported USA Continue
task is checked at 09A5F3D0 and 09A5F4C4. Its selector pointer at 09A44DA0 has
slot index +8, preview state +12, mode +4, and model-buffer pointer +2380.
Only settled state 3/mode 1, an occupied validated slot, and 45 stable frames
permit capture. The guest is read only; no menu state, selected slot or save
bytes are changed. Native tests switched between Luis and Invoker to verify
slot association. Their visible equipped armor and weapons were inspected.

The game's rendered Character Select model panel is cropped into a PNG; its
Rotate hint and bottom prompt are excluded. These are cached images of the
game's preview, including its background, rather than live interactive models.
The renderer and game assets are not started merely to draw the launcher.

Local PNGs live in `<data>/save-preview/portraits/slot-N-KEY.png`. KEY is SHA-256
of the decoded hunter's first 0x68 bytes at slot stride +1000: identity, basic
appearance and six equipped records. Optional `appearance N KEY` lines extend
the existing FUPREVIEW 1 summary without breaking older summaries/backups.
Currency changes retain a portrait; equipment/identity changes select a new
key. A highlighted hunter is recaptured on later Character Select visits,
including texture/color changes. Deleted/uncached portraits show a placeholder.
Imported saves require a fresh validated summary before portraits are shown.
A new or changed outfit may need to be highlighted in Character Select for
about 1.5 seconds before its portrait is available on the next launcher visit.

The current owner's two portraits were seeded from a copied save after verifying
all original/copy save-folder hashes. Only PNGs and the launcher summary were
installed; original saves stayed unchanged. The existing desktop shortcut
already targets the updated build/windows/bin/FreedomUniteNative.exe.
Frozen packages were not replaced.

Five relevant regression targets passed (save editor/portrait identity, save
preview, localization and two camera targets). The save-preview Python fixture
was also run with a locally extracted, undistributed save table and passed
three-slot, Unicode, integrity, stale-summary and read-only cases. It exposed
a pre-existing CRLF summary-header issue; headers/rows now accept Windows
newlines. The standalone editor is rebuilt against the extended summary so
its edits retain portrait keys. No automatic-launch behavior is implemented.

Evidence: build/windows/investigation/launcher-portraits/ contains the build
and test logs, installed-cache/save-hash verification and launcher screenshots.

## Isolated Luis model prototype (15 October 2026)

The owner requested an actual model and selected Luis for the first milestone.
Luis's card now prefers a local `.fum` geometry cache over the PNG. Other hunters
retain their existing portraits. `MHFU_MODEL_PREVIEW=0` restores the PNG path.

After the same guarded Character Select settle period, the profile observes one
frame of decoded GE draws. Only transformed triangle primitives are copied;
screen-space menu artwork is excluded. Vertices are already posed by the game's
bone transforms. World/view transforms establish a consistent initial direction;
the saved data contains triangle positions, normals, UVs, material tint and
decoded textures. Strips sharing textures/material settings are combined. No
framebuffer pixels are used for the model. The generic renderer observer is
read-only and temporarily requests CPU vertex decoding while attached.

`host/hunter_model.cpp` renders this geometry independently, with an orthographic
camera, directional lighting, a depth buffer and a transparent background. Drag
horizontally over Luis to turn him. Rendering follows the card's aspect ratio and
only repeats when its dimensions or angle change. Cache input has size/count,
dimension, triangle and finite-coordinate checks. Texture lifetime follows the
existing ImGui deferred-destruction path.

This is a **fixed-pose mesh prototype**, not a skeletal animation system or a
standalone loader for the game's raw equipment assets. It still refreshes by
highlighting Luis in Character Select, and uses approximate preview lighting.
No game/menu simulation runs behind the launcher. The next larger step would be
direct asset assembly from the save's equipment records and animation support;
automatic hunter loading remains deferred.

Copied-save native runs produced a complete eight-material model (492,672-byte
cache) and front/back launcher views. The rendering tests cover depth ordering,
rotation, texture replacement, transparency, cache round trips and corrupt input.
Evidence and test logs are in `build/windows/investigation/luis-model/`.
Six relevant regression targets passed with the real-format save fixture enabled.
Sandbox restrictions on temporary-file paths required the final suite to run
outside the sandbox. Original and copied save-folder hashes remained unchanged;
only Luis's derived mesh cache was installed into the normal profile. The stable
desktop shortcut points to the rebuilt executable. Mouse dragging is implemented;
automated native captures verified front/back angles using `MHFU_MODEL_YAW`.

### Continuous rotation texture fix

The owner reported font-atlas corruption while dragging. A bounded 300-frame
native run with `MHFU_MODEL_SPIN_TEST=2` reproduced that exact failure; the earlier
constant-angle checks did not exercise repeated texture replacement. Rotation
now retains its texture and queues full pixel updates with
`ImTextureDataQueueUpload`. Texture replacement occurs only when dimensions or
the selected cache change, and unuploaded textures are not submitted for drawing.
The same continuous run completed with correct side/front/back views, and the
model regression target passed. Before/after screenshots and logs are retained
in `build/windows/investigation/luis-drag/`. The stable desktop shortcut uses the
rebuilt executable; no save/cache data needed changing for this fix.

### All three slots

After the owner accepted the rotation fix, the model capture/display path was
enabled for every occupied slot, removing the Luis name restriction. Hovering
over a model now uses the hand cursor. Each of the three cache entries retains
its own mesh, angle and persistent texture; empty slots retain their normal
empty-state card. Models still refresh after a stable Character Select visit,
with PNG fallback until a matching model cache exists.

Native copied-save runs captured both Luis and Invoker, then exercised continuous
rotation with both visible. A separate launcher-only fixture populated the third
card with a duplicate derived model to verify three simultaneous preview caches;
it is not a third real hunter and is never installed in the owner's profile.
Evidence: `build/windows/investigation/all-slot-models/`.

### Drag ownership

Model panels use a per-slot ImGui invisible button to own a drag from the initial
left click through release. Rotation follows that active card even outside its
bounds; crossing another card does not transfer the drag. Clicking elsewhere and
then moving into a model does not start rotation. The hand cursor remains active
during the drag. This replaces the earlier hover-plus-held-button condition.
The button uses ImGui/SDL's existing mouse capture and leaves normal Play
behavior unchanged. Evidence: `build/windows/investigation/model-drag-capture/`.

### Maximizing and texture-pack behavior

The owner's disappearing-preview report was reproduced by maximizing a copied
profile launcher from 960x544 to 1920x1009. The UI Vulkan descriptor pool held
only the eight-image font-atlas minimum. Font/preview resizing retains old GPU
textures briefly while allocating replacements; one preview then reached status
OK with texture ID zero and stayed on the placeholder. The UI pool now budgets
128 sampled images, and backend Vulkan errors are logged rather than silently
ignored in release builds. Input diagnostics support `window maximize` and
`window restore` for repeatable launcher-only coverage. Trace output records
model texture state under `MHFU_PORTRAIT_TRACE`.

At this checkpoint, the model cache decoded original guest-memory textures before
the GE renderer applied replacements. Enabling an HD pack did not change these
3D previews. The earlier framebuffer PNG fallback is
a different path and can include replacements already visible in the game.
The active-pack material integration described below supersedes that limitation.

Evidence: `build/windows/investigation/model-maximize/` contains before/after
resize captures, texture-state traces and a three-card continuous-rotation
fixture. No game entry or save/cache regeneration is part of these checks.
The two-hunter run completed two maximize/restore cycles; a 120-frame three-card
spin run completed a maximize/restore cycle, with all previews visible and no
invalid ready texture IDs or logged UI Vulkan errors. A longer three-card stress
run hit its 55-second bound before its final restore capture; that partial run
is retained separately and is not counted as a completed check.

## Active texture-pack materials (15 October 2026)

All occupied cards now follow the same enabled texture pack and folder selection
as the game renderer. Original materials remain available at all times: disabling
the pack restores them immediately, and absent, excluded or unreadable replacements
leave the affected material unchanged. Geometry, lighting, UVs and drag angles
are preserved while material pixels change.

FUM2 caches add original texture registers, raw texture bytes and palette bytes
to the existing posed geometry and decoded original pixels. `host/model_materials.cpp`
restores these snapshots into isolated guest memory, then reuses PortableKit's
`TexturePack` matcher and decoder. This preserves its hash algorithms, address
rules, palette hashes, wildcard/exclusion rules and hash-range padding. No live
game or save is needed for lookups. FUM1 caches remain readable but need a new
Character Select capture before their materials can be matched to a pack.

One shared worker opens the selected pack and loads replacements away from the
UI thread. Results carry a per-slot ticket and pack revision, so old jobs cannot
overwrite newer outfits or settings. The original scene is immutable. Changing
the enabled flag/folder invalidates materials immediately; INI files and matched
replacement files are checked every half second for edits. Decoded source images
are released after copying; art larger than 2048 pixels per side is reduced for
these small previews. No HD pixels are written into the base model cache.

The two material/model regression targets pass, covering all three slots,
original fallback, corrupt images, pack switching, disabled/stale results,
PNG/INI edits, source-identity serialization and legacy FUM1 rendering. Native
copied-save checks with the owner's installed 489-entry pack matched 1/8 Luis
materials and 6/8 Invoker materials; the remainder kept originals. Enabled and
disabled captures, plus rotation and maximize/restore with HD materials, passed.
Both current model caches were regenerated from copied saves for FUM2 support.

Evidence: `build/windows/investigation/model-hd/`. Original saves and the
standalone save editor are unchanged. Animation/direct equipment-asset assembly
and automatic save loading remain later work.
Only the two derived FUM2 caches were installed into the normal profile after
appearance-key and save-hash checks. Its launcher-only trace confirmed the same
1/8 and 6/8 matches. A long installed-profile run hit its time bound before the
scheduled screenshot; a shorter run captured the UI and exited normally in
6.44 seconds with the owner's graphics settings unchanged. The desktop shortcut
continues to point to the rebuilt executable.

## Launcher Load controls — 15 October 2026

Each occupied card now offers Load above its model. The request follows the
launcher's existing Play path, including its changed-mod warning and normal
backup handling. Returning to the home page cancels any pending request.
Play itself keeps ordinary startup. Empty slots have no Load control; legacy
summaries without an appearance identity need a normal game load to refresh.

`host/direct_load.cpp` advances the USA title/demo and Continue tasks with
normal PSP input. It never changes a save, jumps into guest functions or writes
guest state. Start advances startup; Cross selects the requested slot,
confirms Yes and dismisses Character Selected. The generic framework exposes
only a launcher start request and an optional pad filter; game addresses and
selection rules remain in the FU profile.

The loader checks the decoded save against the requested hunter's name and
appearance key before steering Character Select. It ignores transitional menu
objects until a settled selector has a valid model/save buffer, spaces input
pulses with neutral frames, and checks the selected index again at confirmation.
After acceptance, it sends neutral input until the gameplay overlay is present,
then relinquishes control permanently for that session. Startup menus are
covered by a Loading screen naming the hunter. Esc or manual pad input cancels;
unexpected states and a 90-second timeout return control with a message.
The handoff verifies the appearance hash again against the live hunter payload
at +0x4a0, and recognizes both village/lobby and quest gameplay overlays.
The host's normal menu request is consumed as cancellation only while loading;
ordinary pause-menu behavior resumes after handoff.

`MHFU_DISABLE_DIRECT_LOAD=1` restores the previous cards without Load controls.
`MHFU_LOAD_TRACE=1` logs decisions and guarded menu observations. The navigation,
identity checks and transitional-selector rules are covered by
`mhfu_direct_load_tests`. Native diagnostic runs use copied saves, separate
logs and a maximum 55-second watchdog.

Validation: four relevant regression targets pass (navigation, save previews,
model geometry and material loading). Native clicks loaded Luis and Invoker
into the village with matching live identities and the expected 1,145 / 12,108
Zenny. Invoker's test used normal game speed and completed in 43.60 seconds.
The final build also passed Luis loading followed by opening/closing the pause
menu, Esc cancellation, and ordinary Play without an automatic-load request.
A deliberately mismatched copied summary stopped at decoded-save validation.
Every diagnostic save hash remained unchanged. The installed launcher showed
both green Load buttons and intact models; the branded desktop shortcut still
targets the rebuilt native executable. All four original save files match the
earlier baseline. Evidence: `build/windows/investigation/direct-load/`.

The native game runs end through the scripted window-close request (runtime
exit 4, reason "window closed"); launcher-only runs exit 0. No native run hit
its watchdog. The 90-second timeout recovery is implemented but was not waited
out in native tests. All three target indices have navigation coverage; the
owner's third slot is empty, so a real third-hunter load remains unverified.

### Direct-entry assessment and silent startup — 15 October

The selected-hunter payload copy is at the resident selector's 088BD474 call;
the Continue task handles selector success at 09A5F548. These are internal
stages of an initialized guest, not standalone launcher entry points. They
depend on the decoded selector buffer, allocated live hunter state, task/fade
managers and loaded resources. A faster route could bypass presentation tasks
after required bootstrap initialization, but that needs a separate startup
refactor and native validation. Per the owner's fallback request, the working
loading flow is retained rather than implementing that bypass now.

Automatic Load now uses `AudioSink::set_output_suppressed()` before audio
initialization. This temporary flag is separate from saved volume and mute.
The device opens at zero gain while suppressed; its callback continues retiring
audio normally but submits silence. On arrival, cancellation, validation failure,
timeout or return to the launcher, suppression is removed and the current user
gain resumes. Buffered hidden-screen samples are discarded before resuming, so
startup sounds cannot burst out afterward. Ordinary Play is unaffected. Audio
dumps retain the unscaled guest mix, as before.

Audio-enabled copied-save checks at the diagnostic 20% volume verified zero
device gain at startup and gain 0.2 at Luis's village handoff, restored gain 0.2
on Esc cancellation, retained gain 0 for an already-muted player, and ordinary
Play opening directly at gain 0.2 without suppression. All completed within
their bounds and preserved the copied save hashes. The four relevant regression
targets still pass. Evidence: `build/windows/investigation/direct-load-audio/`.

### Branded loading screen — 15 October

The loading screen now reuses the launcher's mountain/lake background and
Freedom Unbound logo, with a light darkening layer and centered hunter message.
The existing artwork textures are shared; the loading copy is drawn on the
foreground layer so it fully covers guest startup screens. Its logo and text
scale with the window. The launcher layout and credit retain their original
background-layer rendering. An Invoker copied-save run verified the loading
screen at 960x544, maximization to 1920x1009, restoration and the village handoff;
the copied save hashes stayed unchanged. Evidence:
`build/windows/investigation/loading-art/`.

## Launcher layout and equipment details — 15 October 2026

The sidebar starts with navigation, without the redundant project/LAUNCHER
heading. All navigation buttons fit in the available height without a scroll
bar; their faces use a restrained procedural brushed texture. Hover descriptions
are tooltips. Version/status and language flags are anchored near the bottom.
Play sits above the three cards, allowing the cards to span the hero area.

Each occupied card places the six equipped slots on the left, in weapon, helmet,
torso, braces, waist, feet order, and keeps the draggable model on the right.
The details below include both point balances. Narrow gear rows clip their names
without a hover overlay (see the icon follow-up below). Existing drag capture, HD model materials
and Load behavior are retained. The weapon slot uses the game's weapon symbol;
armor slots use the corresponding original armor symbols.

Summary extensions are optional `details INDEX POKKE GUILD` and
`gear INDEX SLOT PRESENT TYPE ID` rows. Legacy summaries remain readable and
show unknown details until the game next reads or writes the save. The points
map to live state offsets `0x696E8`/`0x696EC`, consistent with the game's point
update routines and the measured save/live displacement of `0xB60`.

The decrypted archive entry 19 hook caches equipment names locally as
`save-preview/equipment-names.bin`. Armor name sections are 7, 9, 11, 13, 15;
melee and ranged weapon sections are 3 and 5. The saved armor type enum is feet,
helmet, torso, braces, waist (0–4); the UI reorders the raw records using indices
0, 2, 3, 4, 5, 1. This was checked against both hunters' in-game Status pages.
The read-only draw observer
recognizes the original decoded 256×256 common-menu icon atlas by its measured
pixel hash and caches `save-preview/equipment-icons.rgba`. These files contain
locally derived game assets and must never be committed or packaged. A clean
installation obtains them during game startup; the launcher does not ship game
art or equipment strings. The observer detaches after capture (and is skipped
when the cache already exists), retaining the normal GPU vertex fast path.
The icon follow-up below supersedes this observer lifetime and adds the second atlas.

Synthetic fixtures additionally verify point/gear decoding, summary round trips,
legacy summaries, malformed optional indices and read-only save access.

Validation evidence for this layout is under the ignored
`build/windows/investigation/launcher-refresh/` directory. Native captures were
inspected at 480×272, 960×544 and maximized 1920×1009. All navigation buttons
and bottom controls fit; maximize/restore retains the model previews. A held
Luis drag continued outside his panel and across Invoker without rotating her.
English/Portuguese switching was checked. Both hunters' Status equipment pages
were compared with the saved records, and the final native build loaded Invoker
from the relocated Load button. A clean-cache run regenerated the original icon
and name caches with hashes matching the research originals. All bounded native
runs retained identical copied-save hashes. Save preview, direct-load policy,
text table, hunter model, material loader and save editor regressions passed.

Only derived equipment caches and the refreshed summary were installed in the
owner profile after confirming the current save matched the tested copy. All
original save-folder files retained their hashes. The existing desktop shortcut
still resolves to the rebuilt native executable and its embedded icon.

### Card arrangement follow-up — 15 October 2026

The owner requested swapping the two text groups: hunter name, rank, currencies
and play time now sit to the left of the model. The equipment list sits below
both columns and uses the full card width, preserving weapon/head/torso/braces/
waist/feet order and original icons. Card hover tooltips were subsequently removed at the owner’s request. This supersedes the equipment-left layout above.

The follow-up build was inspected at normal size, maximized and restored;
equipment names now use full-width rows. Copied-save hashes stayed unchanged.


### Exact equipment icons and no hover overlay — 15 October 2026

Save-card equipment and hunter-info hover popups are removed. Icons use the
complete original sprite descriptor rectangle (24×24 for equipment), preserving
aspect ratio and padding so their edges are visible. Weapon type and armor slot
come from the original USA game tables; rarity colors follow the original BGR
palette and RGB555 conversion. The entry-19 hook derives
`save-preview/equipment-styles.txt` locally. The second original 128×128 atlas is
cached as `equipment-icons-extra.rgba`, alongside the existing 256×256 atlas.
None of these derived game data files may be committed or packaged.

The texture-only observer remains until both pages are obtained without forcing
CPU vertex decoding; model capture still requests decoded vertices as before.
The `verify-equipment` diagnostic compared sprite descriptors and colors with
interpreted original game routines: 31 samples, 11 weapon classes and 10 rarities
passed. Native hover, maximize and restore captures were inspected in
`build/windows/investigation/launcher-refresh/icons-final/`; icons are complete,
weapon colors differ correctly and the hover popup is absent. Test and owner
save hashes stayed unchanged. Only derived icon caches were installed into the
owner profile, after matching ELF identities. The stable executable is rebuilt.
