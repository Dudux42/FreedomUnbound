# Freedom Unbound — test build changes

15 October 2026 · `FreedomUnbound-test` · Windows x64

This records the launcher and progression-testing work since the earlier alpha.
The standalone save editor is outside this change log and test package.

## Launcher presentation and controls

- Taller cards support all three hunter slots. Occupied cards show a rotatable,
  fixed-pose equipped hunter; empty slots remain empty.
- Hunter name, rank, Zenny, Pokke Points, Guild Points and play time sit beside
  the model. Equipment uses the full width below, ordered Weapon, Helmet,
  Torso, Braces, Waist and Feet.
- Equipment uses the game's own weapon-class/armor symbols and rarity colors.
  Full sprite rectangles prevent cropped icon edges. Card hover popups are gone.
- Preview rotation uses a hand cursor. Drag ownership stays with the clicked
  card even outside its panel, without rotating neighboring hunters.
- Model previews survive maximize/restore. Materials follow an enabled texture
  pack, with original-texture fallback and refresh when the pack changes.
- Compact textured navigation buttons fit without sidebar scrolling. Redundant
  launcher heading text is removed; version and language controls sit near the
  bottom. The scenery, logo and Windows executable icons are embedded.
- Startup and ZIP drops show/restore the launcher and request foreground focus.
  Windows file and folder requests use native Windows dialogs.

## Starting a hunter

- A Load button above each occupied preview starts that selected hunter and
  advances the normal game startup flow to Pokke Village.
- A loading screen uses the launcher background and logo. Startup audio stays
  silent until arrival, then restores the player's volume/mute preferences.
- Esc cancels automatic startup. Identity checks, guarded menu navigation,
  timeouts and neutral input handoff protect against loading the wrong hunter.
- Play remains available for ordinary startup. This uses the game's normal
  loading flow; it does not bypass the game with a direct village-state restore.

## Progression-testing cheats

- Unlimited health and stamina; one-hit kills.
- Maximum Zenny and replenishment while enabled.
- Material/fee bypass for forging and upgrades. Newly forged weapons, armor and
  decorations keep progression-based availability; upgrades retain the bypass.
- Attaching decorations lists only decorations the hunter actually owns.
- Item-box deposit stack multiplication for faster testing.
- Optional quest completion with a fresh L+R press during a supported active
  quest. Normal quest rewards are retained in the tested gathering quest.
- Cheat preferences persist separately. Reset/restart restores normal rules
  after disabling cheats that require it. Previously changed balances, crafted
  gear, inventory and saved progression remain ordinary save contents.

## Mod and runtime integration

- Launcher model materials integrate with installed/enabled texture mods.
- Shared host hooks and optional providers support the existing experimental
  camera, damage, gathering, monster, key-quest and shared-progression work.
  Experimental mod bundles are not installed or enabled by this package.
- Text-mod handling and host translations remain integrated with the launcher.
- Texture-only equipment observation preserves the GPU vertex fast path;
  model capture still requests decoded geometry when required.

## Validation and remaining coverage

The owner accepted the cheat recipe/decoration behavior, preview rotation,
card drag ownership and selected-hunter startup. Desert gameplay and encountered
monster cutscenes with synchronized audio were reported working. Native checks
covered both occupied hunters, hover, normal/maximized/restored layouts, Windows
activation/dialogs and copied-save integrity. Original icon/color routines
matched 31 equipment samples covering 11 weapon classes and 10 rarities.

These results do not establish full campaign, every weapon/monster/map,
multiplayer or second-PC compatibility. Gameplay remains 30 FPS; higher renderer
rates use experimental interpolation, with the known hunter-house background
issue still outstanding. See `MASTER_PROJECT.md`, `SAVE_PREVIEW.md` and
`CHEATS.md` for detailed evidence and limitations.

## Test package

Extract the complete folder and run `FreedomUnbound.exe`. Supply your own
supported USA game image during setup. The package includes the native runtime,
298 compiled overlay libraries, dependency notices and launcher translations;
it contains no ISO, extracted executable image, personal saves, texture mods,
or locally cached original equipment artwork/model data.

## Package regression checks

The standard release build passed. All 52 CTest targets passed together in the
confirmation run, and the separate save-preview fixture with the locally derived
game table passed. 