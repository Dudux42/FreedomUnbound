# Freedom Unbound — Alpha 0.1.5

19 October 2026 · Windows x64 · `v0.1.5-alpha`

## Display and texture packs

- **Fill** aspect ratio: widescreen, 16:10 and resized windows show a wider 3D
  view instead of falling back to Original. Menus and other 2D screens keep
  their shape.
- **F10** switches the installed HD texture pack off/on, and only while a
  texture pack is installed; without one, F10 does nothing.
- Texture warmup: when a quest area or the village/lobby starts, textures that
  scene used on earlier visits are prepared before drawing, within a time and
  memory budget, with the normal loading as fallback.

## Keys and in-game panels

- **Equipment Wishlist (F7):** an in-game panel for the Equipment Wishlist mod,
  with crafted-item notices. F9 from the panel switches to the mod manager.
- **F7** is reserved only while the Equipment Wishlist mod is on, and **F10**
  only while a texture pack is installed; otherwise you can bind them to game
  controls. **F8** is free for your own bindings. Esc, F3, F9 and F12 stay
  reserved.

## Saves and launcher

- Save-screen error messages and the **Game data** label follow the launcher
  language, including Brazilian Portuguese.
- The application name is **FreedomUnbound**. The standalone save editor checks
  both executable names, `FreedomUnbound.exe` and `FreedomUniteNative.exe`, to
  see whether the game is running.
- Installing a mod version that is already installed now says **mod is already
  installed**. Installing an older version asks **A newer version is already
  installed. replace it?** with Yes/No (No is selected first). Both apply to
  dropped ZIPs and to catalog installs.
- As before, the **Load** button keeps sound off under the loading screen and
  turns it back on in the village, with your volume and mute settings.

## Mods

- Mods currently switch on built-in features only. Texture packs and text mods
  are fully supported, and anyone can make them; new features or game logic
  can't be added as third-party mods yet. A plugin system that lets mods add
  their own features is coming.
- Shared Guild Progression writes its changes into your save, so turning the
  mod off later doesn't undo progress you've already saved.
- pt-BR accented letters now match the game's font in the pt-BR mod 0.2.3.

## Known issues

- HD textures can still pop in on first visits: the warmup currently prepares
  2 of the 33 known village textures in time.
- Experimental interpolation can still distort the background inside the
  hunter's house; use 30 FPS if this happens.
- The Gathering Map options and the Equipment Wishlist close hint are shown in
  English only.
- The LAN hunt is not fully tested: completed multiplayer quests, rewards and
  save/reload on separate PCs are still pending.

# Freedom Unbound — Alpha 0.1.4

17 October 2026 · Windows x64 · `v0.1.4-alpha`

## Mod downloads and organization

- Download mods directly from the launcher, with progress, cancellation, exact
  size/SHA-256 verification and the normal install review.
- The built-in index is named **Freedom Unbound Official Mod Page**.
- Add public GitHub indexes; saved sources appear in separate expandable groups.
  Click a heading to show or hide its mods. Each source has its own refresh,
  error handling and download cache; custom sources can be removed.
- Installed versions are shown. New mods remain off until enabled; replacing an
  installed version keeps its enabled state. ZIP drag-and-drop still works.
- Move up/down actions have been removed from the installed mod list.

## In-game mod controls

- **F9** opens the built-in in-game mod manager. Supported gameplay mods can be
  toggled live; restart-only packs and save-affecting mods are identified.
- Gameplay keeps running while the panel is open, with gameplay input captured
  while navigating it. Close with F9, Esc or the controller Back button.
- Gathering Map includes independent gathering/mining/bug/carving options.
- **F10** switches the installed Freedom Unbound HD texture pack off/on during
  gameplay and saves that choice. The mod pack itself is a separate download.
- Includes the current native support for buff indicators, equipment material
  pinning, map markers, controller prompts, return timers and the other supported
  optional mods. These providers do not enable or bundle the mod ZIPs.

## Network and launcher

- Pre-game LAN Host/Join controls and network status are available in Settings.
  Join the same Online Gathering Hall number on every participating PC.
- Keeps the rotatable equipped-hunter previews, HD preview materials, equipment
  icons/colors, per-hunter loading, native Windows dialogs and foreground fixes.
- Keeps the progression-testing cheats, including progression-aware crafting,
  owned-decoration lists and optional L+R quest completion.

## Setup and current limits

Extract the complete Windows x64 ZIP and run **FreedomUnbound.exe**. You need a
Vulkan-capable graphics driver, an AVX-capable CPU, the Microsoft Visual C++ x64
runtime and your own supported USA PSP game image (ULUS10391). Keep all adjacent
DLLs, overlay libraries and startup scripts together.

Gameplay is 30 FPS. Higher presentation rates use experimental interpolation;
the hunter-house background issue remains known. Full campaign, all weapons and
monsters, broad PC compatibility, and internet/cross-emulator multiplayer are
not established. LAN Hall visibility/movement and transport checks have passed;
completed multiplayer quests, rewards and save/reload are still pending in the
recorded LAN validation. See LAN_MULTIPLAYER.md and the feature guides.

This package contains no game image, extracted original executable, personal
saves, installed mod packs or game-derived preview caches. The standalone save
editor is not included.

## AI-use disclosure

Freedom Unbound uses AI-assisted workflows for development, technical research,
debugging, testing support, documentation and some original project artwork.
AI-assisted work can contain errors. The project's recorded tests and known
limitations describe what has actually been checked; AI assistance does not
establish complete gameplay compatibility or an absence of bugs.

## Legal disclaimer

**Freedom Unbound** is an independent project and is not affiliated
with, authorized by, sponsored by, or endorsed by CAPCOM, Sony, or any of their
affiliates.

Monster Hunter, Monster Hunter Freedom Unite, CAPCOM, PlayStation, PSP, and all
related trademarks, game assets, artwork, audio, characters, and other intellectual
property belong to their respective owners.

**Freedom Unbound** does not distribute the original game files or game assets:
no disc image, original PSP executable or data files, and no original game
textures, models, audio or video. You must provide the required files from your
own legally obtained copy of **Monster Hunter Freedom Unite for PSP**. The
installer verifies the supported original **USA release (ULUS10391)**; other
releases are not currently supported.

Users are solely responsible for obtaining, dumping, extracting and using their
game copy in accordance with the laws applicable in their jurisdiction.

**Freedom Unbound** does not support, provide, link to, or encourage the use of
unauthorized or pirated copies of the game.

References to the original game and its trademarks are made solely for
identification, compatibility and interoperability purposes.

Screenshots and other depictions of the original game may be used to document
or demonstrate Freedom Unbound's functionality. All depicted third-party game
content remains the property of its respective rights holders.

Any license covering Freedom Unbound's own original code and materials does
not grant rights to third-party intellectual property. Third-party components
retain their respective licenses and notices.

**Freedom Unbound provides the software, not the game. You must provide your
own legally obtained copy.**
