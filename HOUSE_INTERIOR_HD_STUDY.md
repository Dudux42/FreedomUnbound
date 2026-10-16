# Player house interior HD: reference study and authoring workflow

11 October 2026. Scope: floor, walls, ceiling, and supporting beams where they
belong to those surfaces. This study does not establish their texture identities
or implement an HD pack.

## Reference inspected

[three5media/mfhu-hd-retexture](https://github.com/three5media/mfhu-hd-retexture)
at commit `18b3ee4f4c0074acd9e36ef4330a146cf59cbe2b`.

The [README](https://github.com/three5media/mfhu-hd-retexture/blob/18b3ee4f4c0074acd9e36ef4330a146cf59cbe2b/README.md)
describes mostly Cupscale upscales using multiple models, supplemented by
replacement artwork and contributions from other packs. It targets the US
release. Its stated scale is 4x for environments and 8x for characters,
equipment, and monsters. PNG optimization reduced the distribution size.
The author permits reuse, but identifies contributed artwork separately.
Specific model names, per-image settings, original inputs, and a reproducible
house-interior recipe are not supplied by this README.

The method we can adopt is upscale first, then selectively repair or recreate
weak details while preserving the game's visual identity. We cannot claim to
reproduce their exact processing from the published description.

## Direct asset and mapping inspection

Inspected the pinned repository tree, complete
[texture mapping file](https://github.com/three5media/mfhu-hd-retexture/blob/18b3ee4f4c0074acd9e36ef4330a146cf59cbe2b/ULUS10391/textures.ini),
and all 179 village PNGs (30,045,008 compressed bytes). Four labeled contact
sheets and a dimension inventory are local under `build/texture-study/`.

The pack uses `hash = quick`, numbered village images, broad keys often based
only on the data hash, and a separate hash-range section. There are 171 village
mapping lines naming 159 distinct files; 20 village PNGs are not explicitly
referenced there. Two village keys occur more than once with different paths.
Some different keys deliberately name one image. File count therefore does not
measure unique coverage. The examined village dimensions vary from 128-square
to 1024-square, with two irregular sizes; this is not a uniform final-size set.

Visible material candidates, **not confirmed house assignments**:

| Reference file | Dimensions | Appearance |
| --- | --- | --- |
| `areas/village/49.png` | 512 x 512 | Dark wood planks |
| `areas/village/66.png` | 512 x 512 | Similar dark wood planks |
| `areas/village/64.png` | 256 x 256 | Light plank panel with a beam |
| `areas/village/97.png` | 512 x 512 | Wood framing with pale infill |
| `areas/village/99.png` | 512 x 512 | Broad weathered boards |
| `areas/village/116.png` | 256 x 256 | Pale boards |

Visual examination shows both single materials and sheets containing multiple
objects. Some wood images have pronounced shading already baked into them.
Any new processing should preserve that shading rather than add a second
lighting direction. Filenames alone do not identify floor, wall, or ceiling.

## How this transfers to our project

Reviewed `portablekit/host/gpu/texture_pack.cpp`, `texture_pack.hpp`,
`vulkan_renderer.cpp`, `replacement_textures.cpp`, the pack composer, and
`TEXTURE_PACK_WORKFLOW.md`.

- Our loader accepts xxh32 and xxh64, and rejects quick. This reference pack
  cannot be imported unchanged. Editing the hash setting does not convert
  existing keys: they must be measured again from original texture data.
- The existing renderer can dump decoded original PNGs with
  `MHFU_TEXTURE_DUMP`. With no pack loaded, its default options use xxh64 and
  ignore memory addresses. With a pack loaded, dumping uses that pack's options.
  Collection must therefore begin with original artwork and no selected pack.
- The dumper creates an INI and hash-named PNGs. Our authoring pack should instead
  have readable filenames and explicit 24-digit mappings, as required by the
  composer. Preserve the measured palette/dimension portion of each key.
- PNG replacement loading, mip generation, optional filtering, launcher
  selection, and original fallback already exist. No renderer feature is
  presently demonstrated to be missing for the house pilot.
- The composer accepts a narrower format than the renderer: flat explicit
  base-level mappings with no hash-range sections. Our small controlled pack
  should fit that format rather than inherit the community pack's full INI.
- The dump queue can drop images, and a dropped key is marked seen for that
  process. A new process is needed for a retry; walking around again in the
  same session is not a reliable recovery.
- Dumping disables one background decoding path. Performance comparisons must
  run separately with dumping disabled.

## Concrete house workflow

### 1. Collect a baseline

Use the prepared native build with an isolated copy of the test save. Run a
bounded collection session and enter the house without changing equipment or
decorations. Keep game-derived images, captures, and working art under ignored
`build/texture-study/house/`, in accordance with `GROUND_RULES.md`.

Capture repeatable views from the doorway, middle, and opposite side. Inspect
which ceiling surfaces are actually visible with the current camera. Capture
the room empty of menus; note any camera angle needed to inspect beams.

Set dumping to a fresh `originals/` directory, use no loaded replacement pack,
and retain the generated INI. Check the log and resulting images. Repeat in a
fresh bounded process if collection is incomplete. Leave player saves intact.

### 2. Establish texture identities

Make a labeled contact sheet of our originals and compare it with the room
captures and reference candidates. Confirm uncertain matches with a disposable
diagnostic pack, highlighting one candidate at a time in a bounded native run.
Keep diagnostic images separate from final artwork.

Record measured key, source dimensions, visible surface, sheet subregions,
transparency, tiling, and known shared uses in an inventory. Do not label a
reference image as the house floor merely because its boards look plausible.

The selection gate is a verified list of textures/subregions covering the
floor, walls, and visible ceiling. The asset count comes from this evidence,
not a fixed promise of three images or 8-12 images.

### 3. Establish the art recipe on the floor

Start from our original floor image. Produce restrained 2x and 4x candidates
and record the actual tool/model version, settings, source checksum, and output
checksum. Tool selection remains open until the available original is examined;
the reference's use of Cupscale is not a requirement to use that exact tool.

Preserve board placement, cracks, wear, colors, and existing shadows. Avoid
invented nails or grain, changing board widths, and excessive sharpening. For
tiled textures, compare opposite edges and inspect repeated copies. For shared
sheets, preserve the complete layout; restrict edits to confirmed target regions
if other objects are outside scope. Check alpha separately where present.

Evaluate both candidates in the game at the same rendering resolution and
filtering settings. Use the preferred result to establish the house's material
style, then apply that recipe to walls and ceiling, adjusting where necessary.

### 4. Package and verify

Create a versioned house-only pack containing PNGs and explicit measured
mappings. Start with xxh64, ignoreAddress true, reduceHash false, and Auto
filtering; retain compatible collection settings. Do not copy reference keys.
Validate the pack with existing tools before importing it into the isolated
test installation. Keep authored sources separate from composed output.

Verify matched before/after screenshots, moving views, seams, sheet boundaries,
transparency, replacement loading, and room exit/re-entry after a restart.
Inspect shared uses outside the house when identified. Confirm original-artwork
fallback. Measure memory and frame behavior with dumping off and unchanged
render settings. PNG disk compression savings are not GPU-memory savings.

Success means the intended surfaces improve coherently without unintended
object changes, conspicuous shimmer, broken mappings, or unacceptable load cost.
The final evidence is gameplay comparison, not just a clean enlarged PNG.

## Status and next step

Completed: reference inventory, visual examination, INI analysis, compatibility
review, and this workflow. Research assets are isolated in an ignored folder.
No game was launched, no player save changed, no replacement art authored, and
no runtime code or launcher setting changed during the study.

Not yet verified: exact house textures, original-to-reference correspondence,
per-image enlargement factors, reference model settings, actual house coverage,
or gameplay performance of the proposed artwork.

Next: collect the original house interior and confirm its surface mappings.
Only then begin the floor's 2x/4x art trial. A full-game pack conversion or support
for the legacy quick hash is unnecessary for this pilot.

## 11 October authoring follow-up

The owner supplied two house screenshots and authorized the first batch.
Native collection now confirms two 64-square material textures. A red/green
diagnostic pack establishes that the wood is shared across the floor, ceiling
panels, lower walls and fixtures; plaster covers the pale walls. Structural
beams and patterned posts remain outside the first batch.

Two faithful restorations were authored with built-in ImageGen from our original
dumps, without borrowing reference artwork. Exact prompts, generated masters,
checksums, 2x/4x packs and gameplay captures are under ignored
`build/texture-study/house/`. Both pack resolutions pass composition validation.
The 4x native run loaded both replacements, and inspected house captures show
the intended changes. The isolated copied save hashes remained unchanged.
See that folder's README and CHECKPOINT for current verification details.

This first pack replaces shared materials wherever their keys occur; it does
not promise isolation to the house. The user's live game installation and
settings were not modified. A one-time continuation in this chat is scheduled
for 09:40 America/Sao_Paulo on 11 October 2026 to finish only outstanding work.

## 11 October V2 refinement

Following owner feedback, V2 restores the original four-board divisions rather
than V1's extra plank seams, darkens plaster toward neutral light grey, and
adds structural beam wood. Native diagnostics isolated the wood rectangle on
a third shared atlas; its unrelated artwork and original alpha are preserved.
The 4x native test decoded and uploaded all three replacements, two room views
were visually inspected, and copied saves remained unchanged. The 2x pack also
completed its native check: all three replacements loaded, the room capture
was inspected, and copied saves remained unchanged. V2 is complete. See the ignored house
folder's CHECKPOINT.md and README-v2.md for exact progress and deliverables.
The user's live installation remains unchanged.

## 11 October V3 art-direction pass

The owner approved a substantial plaster concept with fewer cracks and new
floor, beam and roof artwork. New masters are authored under the ignored house
study directory. Roof boards visible from inside share the floor material; the
red diagnostic establishes that coverage. This pass does not claim a separate
exterior roof skin. Floor seam layout and unrelated beam-atlas content remain
preserved. An initial wall read too finely in gameplay, so larger worn patches
were authored and the palette refined to neutral grey. Final release native
verification completed with all three 256-square PNGs loaded, two inspected
room views and unchanged copied saves. V3 is complete; the main deliverable
is house-v3-release-4x-validated. The 2x alternative passes composition and
pixel invariants without a V3 native run claim. CHECKPOINT.md and README-v3.md
contain evidence and usage. The user's installation remains unchanged.

## 11 October V4 — 8x detailed materials

The owner requested 8x textures and a more detailed design. New ImageGen edits
add plaster workmanship, detailed growth grain and beam tool marks. The pack
house-v4-release-8x-validated contains three 512-square PNGs and passes
composition plus original-alpha, unrelated-atlas and plank-layout checks.
The bounded isolated native run loaded all three replacements and preserved
copied saves. Internal-scale-4 automatic captures at 1920x1088 were inspected
from two room views. V4 is complete. README-v4.md, CHECKPOINT.md, exact prompts,
masters and previews remain under ignored build/texture-study/house. Previous
versions remain available; the user's installation and settings are unchanged.

## 11 October V5 — furniture and household objects

The owner approved V4 and authorized the next room-object batch. A bounded
six-key colour diagnostic established indoor coverage. Four new source sheets
restore bed/cabinet/chair, chest/chain, hide/bedding fur and household utensils,
curtain/mat/books. V4 floor/plaster files and beam wood are retained exactly.
The combined V5 pack has six 512-square replacements, passes composition/alpha
checks, and loaded all six in a bounded native run. Two room views were visually
inspected and copied saves remained unchanged. V5 object batch is complete;
turquoise ornament overlays, plant leaves and animated fire remain original.
Deliverable house-v5-release-8x-validated, README-v5.md, exact ImageGen prompts,
masters and gameplay previews remain under ignored build/texture-study/house.
The user's installation/settings remain unchanged.

## 11 October V6 — requested object refinements

The owner requested a darker grey kettle, improved chain, darker brown bed
timber, richer bedding, a better item-box rug and entrance ground. A bounded
region diagnostic isolated the exact UV patches. New ImageGen artwork targets
these surfaces; original-alpha/preservation checks retain approved floor,
plaster, beam wood, cabinet and main chest regions. The chain-only strip gains
new transparent open links. Shared materials also cover the chair, central rug,
some fittings, bedside mat and hearth bed.

The combined house-v6-final-8x-validated pack contains seven 512-square PNGs.
Final bounded native verification loaded all seven, two gameplay views were
inspected and copied saves remained unchanged. V6 is complete. README-v6.md,
exact prompts, provenance, checkpoints and gameplay comparison remain under
ignored build/texture-study/house. The user's installation remains unchanged.

## Approved V6 launcher installation

At the owner's request, V6 was shipped under ignored build/mods/House-Interior-HD-V6-8x and enabled in the Launcher.cmd and Cheat-Playtest.cmd profiles. Both prior settings files were backed up and save hashes remained unchanged. A bounded isolated launcher check visually confirmed the Mods page shows the named pack path and replacements On. Standalone ZIP: build/mods/House-Interior-HD-V6-8x.zip. No executable rebuild was needed.


## Executable-adjacent texture layout

At the owner's request, active V6 textures now live in texture/HD texture beside the normal and cheat-playtest executables. PNGs sit directly beside textures.ini. Launcher profile paths point to those folders; a bounded launcher check confirmed all seven mappings load from the normal executable's adjacent folder. Settings backups and saves are preserved.


## Kitchen V1 and desktop save profile

Owner authorized kitchen retexturing at the approved8x style. Rear bedroom doorway is the verified kitchen test route; grey fast-travel entry was initially misread. A kitchen dump and colour diagnostic isolated9 material/prop sheets. New ImageGen artwork for timber, furniture, shelves, oven/stove, straw floor, food props and blue recipe-board cloth combines with unchanged V6 bedroom sheets. Native run loaded all16 images, two room views were inspected and copied saves remained unchanged. Combinedpack is installed in executable-adjacent texture/HD texture folders; timestamped bedroom-only backups preserved. Details and previews: ignored house README-kitchen-v1.md and KITCHEN_CHECKPOINT.md.

Save audit confirmed both saves valid: desktop shortcut formerly selected Test in cheats-playtest, while screenshots used Luis in AppData. Owner selected Luis; desktop shortcut now invokes packaged executable with default Luis profile. Old shortcut/settings backed up; both save files untouched. No save corruption was found.


## Pokke Village V1 — October12

Kitchen V2 background detail was completed and installed before village work.
Twenty-one faithful8x village material sheets now cover wood/doors/fences,
plaster facades, snow-covered roofs, ground/grass blends, cliffs, landmark rock,
mountains/clouds, vegetation, trader goods and bundled cargo. Original UV regions
retained; foliage sprites receive clean detailed alpha cutouts after native QA.
Combined40-key pack retains previous19 PNGs byte-for-byte. Native isolated release
run loaded26 encountered mappings across eight inspected exterior views; saves
unchanged. Installed in both executable-adjacent texture/HD texture folders with
prior kitchen pack backups. Audit/census/provenance are ignored under
build/texture-study/pokke/audit-v1 and authored-v1. All72 captured keys accounted
for, including actor/UI/effect exclusions and six unresolved auxiliary originals.
The owner-requested expired12:47 automation was deleted; no wakeup remains.

## Pokke Farm — FU retextured0.2

Owner requested Pokke Farm work after reviewing village textures. A bounded
isolated Luis survey identified30 novel sheets;24 environment sheets received
faithful8x artwork, with original40 approved PNGs preserved byte-for-byte.
Fine plant cutouts preserve original UV positions and avoid coarse alpha edges.
Native QA corrected a blue/white mountain sheet initially mistaken for water.
Final6x native run loaded all24 farm keys (50 encountered combined mappings),
seven views inspected and copied saves unchanged.64-key ZIP FU-retextured-0.2
passes archive/image integrity checks and includes stable id fu-retextured,
author DuduX42, type textures and game_id ULUS10391. Importable package and
ignored visual audits/provenance under build/mods and build/texture-study/farm.
Current user-installed0.1 selection was not modified during this artwork pass.
