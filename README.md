# Freedom Unbound — Alpha 0.1.5

Created by **Team Wyvern Works**

A Windows native recompilation of **Monster Hunter Freedom Unite**, built on [PortableKit](https://github.com/TeamGDB/PortableKit).

Play Freedom Unite on PC with a dedicated launcher, configurable controls, save management and optional mods. You need your own supported USA copy of the game; game files and saves are not included.

## Current stage — Alpha 0.1.5

**Playable alpha.** Various Village Elder quests have been tested through **★3**, including progression, hunting, saving and reloading. The full game is expected to be playable, but a complete playthrough has not yet been verified.

Current features:

- Native PC gameplay at the original **30 FPS**, with optional experimental interpolation.
- A dedicated launcher with English and Brazilian Portuguese interface options.
- Three save cards with rotatable equipped hunters, equipment details, rank, Zenny, Pokke/Guild Points and play time.
- A **Load** button for each occupied card to start that hunter.
- Automatic and manual save backups, restore, and compatible PSP save import/export.
- Configurable keyboard, mouse and gamepad controls.
- **Fill** aspect ratio: widescreen, 16:10 and resized windows show a wider 3D view; menus and other 2D screens keep their shape.
- Optional texture, text and supported gameplay mods, with verified downloads from the launcher.
- An in-game mod manager (**F9**), a texture pack toggle (**F10**) and an Equipment Wishlist panel (**F7**) while its mod is enabled.
- Texture warmup: texture packs prepare textures a quest area or the village/lobby used on earlier visits before drawing, reducing pop-in.
- LAN Host/Join controls for the Online Gathering Hall.
- Optional cheats for progression testing.
- Screenshots, update checks and a built-in bug report tool.

**Known issues:** experimental interpolation can distort the background inside the hunter's house. Use 30 FPS if this happens. HD textures can still pop in on first visits; the warmup currently prepares 2 of the 33 known village textures in time. Brief loading hitches may occur. The Gathering Map options and the Equipment Wishlist close hint are shown in English only. LAN connections and player movement are working; full multiplayer quest completion, rewards and save/reload validation remain pending. Internet play, cross-emulator compatibility and broader hardware coverage are unverified.

## What's new

- **Fill** aspect ratio is now available: widescreen, 16:10 and resized windows show a wider 3D view instead of falling back to Original.
- **F7** opens the Equipment Wishlist panel while the Equipment Wishlist mod is enabled, with crafted-item notices. **F9** from that panel switches to the mod manager.
- **F10** toggles an installed texture pack and does nothing if none is installed. When the Wishlist mod is off, **F7** is no longer reserved. It stays the Extra right default, but you can rebind it. When no texture pack is installed, **F10** is free for your own bindings.
- Texture packs prepare textures already seen in a quest area or the village/lobby before they are first drawn.
- The **Game data** label and most save error messages on the **Saves** screen follow the launcher language, including Brazilian Portuguese.
- Installing a mod version that is already installed now says so. Installing an older version asks before replacing the newer one (No is selected first). This applies to dropped ZIPs and to downloads.
- The launcher, window title and logs use the **Freedom Unbound** name.
- Brazilian Portuguese accented letters now match the game's font in the pt-BR mod 0.2.3.

Mods are separate downloads through the launcher or
[Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods).
See [release notes](RELEASE_NOTES.md) for changes and current limitations.
LAN quest/reward/save validation, internet/cross-emulator play, broad hardware
coverage and a complete campaign playthrough remain unverified. Experimental
interpolation can distort the background inside the hunter's house.

## Next steps

1. Broaden testing across quests, weapons, monsters, maps and different PCs.
2. Fix reported gameplay, save and stability issues.
3. Improve interpolation, loading and launcher usability.
4. Validate full LAN hunts, study internet/cross-emulator play and continue native 60 FPS research.
5. Consider support for more platforms.

These are development goals, with no confirmed release dates.

## Optional mods

Find my Freedom Unite mods, downloads and installation guides in [Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods).

**No mods ship with 0.1.5.** The download contains no texture packs, text mods or gameplay mods; every mod is a separate, optional download.

| Mod support | What it does |
| --- | --- |
| Texture packs | Replace supported game textures, including HD artwork. Enable one texture pack at a time. |
| Text mods | Replace supported game text and fonts. The launcher's language setting is separate from the game's translation. |
| Gameplay and HUD mods | Turn on supported features built into Freedom Unbound, such as damage numbers, gathering indicators, monster health, camera controls and key-quest markers. These require a compatible launcher version. |

**About mods right now:** under the current approach, mods only switch on features already built into Freedom Unbound, or add data. Texture packs and text mods fully work, and anyone can make them. Mods can't add new features or game logic yet. A plugin system that lets mods add their own features is coming soon.

Browse and download mods directly from the launcher. The **Freedom Unbound Official Mod Page** is included; you can add custom public GitHub indexes and expand or collapse each group. Downloads are checked against the index's size and SHA-256 before installation review.

To install a ZIP manually, **do not extract it**. Drop it onto the launcher's **Mods** page, review the details, choose **Install**, then turn it on.

Press **F9** to manage supported mods in-game. **F10** toggles an installed texture pack, such as the Freedom Unbound HD pack. **F7** opens the Equipment Wishlist while its mod is enabled. New installs remain disabled until you enable them. See [Mod downloads and indexes](MOD_CATALOG.md) and [In-game mod manager](INGAME_MOD_MANAGER.md).

Mods are downloaded separately. Check each mod's guide for launcher requirements, controls and known limitations. Some mods change saved progress. Shared Guild Progression, for example, writes its changes into your save, so turning the mod off later won't undo progress already saved.

For available packages and individual installation guides, visit [Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods).

## Download and play

Download the Windows x64 ZIP from the [release page](https://github.com/Dudux42/FreedomUnbound/releases). Choose the playable package under **Assets**, rather than GitHub's automatic Source code download.

**Requirements:** Windows x64, a Vulkan-capable GPU and driver, an AVX-capable CPU, the Microsoft Visual C++ x64 runtime, and **A LEGALLY OBTAINED DUMP of Monster Hunter Freedom Unite — USA version (`ULUS10391`), in ISO format**. You must supply your own dump; game files are not included.

1. Extract the complete ZIP into a writable folder.
2. Keep all extracted files together and run **`FreedomUnbound.exe`**.
3. Follow first-run setup and select your supported USA ISO.
4. Adjust your settings, then choose **Play**. If you already have a hunter, you can use its **Load** button.

Do not run the game from inside the ZIP or copy only the launcher executable. `Start-Freedom-Unbound.cmd` is available as a fallback.

**Already installed? You can update directly through the launcher.**

## Technical specifications and settings

| Setting | Explanation |
| --- | --- |
| Resolution | Auto follows the window size. ×1–×6 increase internal resolution; higher values give a sharper image and require more GPU power. |
| Fullscreen / Window size | Choose fullscreen or a resizable window and set its starting size. |
| Aspect ratio | Original preserves the PSP picture's proportions. Stretch fills the window by stretching it. Fill fills widescreen, 16:10 and resized windows by showing a wider 3D view, instead of falling back to Original; with Fill, the width follows the window's shape. |
| Scaling filter | Sharp preserves hard pixel edges; Smooth softens the image when it is resized to the window. |
| Texture filter | Choose sharper or smoother sampling of the game's textures. |
| Vsync | Synchronizes presentation with your display to reduce tearing. |
| Frame rate | Gameplay runs at 30 FPS. Higher presentation rates use experimental interpolation; they do not make gameplay native 60 FPS. |
| Lower when behind | Automatically reduces the presentation rate if the chosen rate would slow the game. |
| Game speed | Normal keeps the game at its intended speed. Unlimited lets it run faster where possible. |
| Fast loading | Allows faster processing during supported silent loading sections. |
| Performance | Shows FPS and performance information. F3 toggles the on-screen display. |
| Audio | Adjust overall volume or mute without losing your volume setting. |
| Button bindings | Remap supported keyboard, mouse and gamepad buttons under Controls. |
| Sticks and triggers | Adjust stick dead zones and the trigger activation point. Higher dead zones help prevent drift. |
| Camera | Configure right-stick behavior, supported camera options, speed, inversion and axis swapping. Some camera features require the matching mod. |
| Mouse | Enable mouse control and adjust sensitivity and inversion. |
| Confirm button | Choose the bottom or right face button for confirmation. |
| Name entry | Choose the on-screen keyboard or a preset name when the game requests one. |
| Touch controls | Adjust on-screen controls, size, opacity and camera speed when using a touch screen. |
| Language | Choose English, Brazilian Portuguese or automatic selection for the launcher and host menus. This does not translate the entire game. |
| Restore defaults | Reset the settings on the selected page. |

### Default keys

Default keyboard and mouse bindings (change them under **Controls**):

| Game action | Default |
| --- | --- |
| Move forward / left / back / right | W / A / S / D |
| △ (Triangle) | Mouse Left |
| ○ (Circle, confirm) | Mouse Right / F |
| × (Cross, back) | Space |
| □ (Square) | E |
| L | Q |
| R | Left Shift |
| START | Enter / Tab |
| SELECT | Backspace |
| D-pad up / left / down / right | Arrow keys |
| Camera up / left / down / right | I / J / K / L |
| Extra left trigger (mod action) | F6 |
| Extra right trigger (mod action) | F7, which also opens the Equipment Wishlist when that mod is on |

Freedom Unbound keys:

| Key | Action |
| --- | --- |
| Esc (or L3+R3 on a gamepad) | Open the in-game menu. Always reserved. |
| F3 | Toggle performance information. Always reserved. |
| F7 / RT | Equipment Wishlist (while that mod is on) |
| F9 | Open the in-game mod manager. Always reserved. |
| F10 | Toggle an installed texture pack; does nothing and is free for your own bindings if none is installed. |
| F12 | Take a screenshot. Always reserved. |

Esc, F3, F9 and F12 can't be bound to game actions.

Default gamepad buttons (Xbox / PlayStation):

| Game action | Default |
| --- | --- |
| ○ (confirm) | B / Circle |
| × (back) | A / Cross |
| △ | Y / Triangle |
| □ | X / Square |
| L | LB / L1 (also LT / L2) |
| R | RB / R1 (also RT / R2) |
| START | Start / Options |
| SELECT | Back / View / Share |
| D-pad | D-pad, and the right stick by default |
| Analogue stick | Left stick |
| Camera | Right stick, when the modern camera is on |
| In-game menu (Esc) | L3 + R3 together |

- The **Confirm button** setting swaps ○ and ×, and the menus follow it.
- When a mod uses the extra keys, LT / L2 is Extra left and RT / R2 is Extra right.
- L3, R3 and Guide can't be rebound.
- F3, F9, F10 and F12 are keyboard only.

**Classic layout:** the default keyboard layout is WASD plus the mouse. To switch, open the in-game menu (Esc or L3+R3), go to **Controls** and choose **Use the classic keyboard layout**. It copies in the keys from earlier versions: move I J K L, △ S, ○ X, × Z, □ A, L Q, R W, START Enter, SELECT Right Shift or Backspace, and the D-pad on the arrow keys. There's no toggle back; to undo it, rebind the keys.

The launcher can open your screenshots folder.

Use **Saves** to manage backups, restore or import/export a compatible save. Keep a separate copy of your original PSP save and follow any restart prompts.

Optional **Cheats** include unlimited health/stamina, one-hit kills, maximum Zenny, item-box stack multiplication, free crafting for available recipes and L+R quest completion. Some cheats require a restart after disabling. Money, items, crafted equipment and saved progress are not undone when a cheat is turned off.

To report a problem, use **Report a bug** in the launcher and share the report through the server's bug-report channel. Include what happened and how to reproduce it. The report excludes your saves, save keys and game files.

## Project documentation

- [Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods): my FU mods, downloads and player guides.
- [Mod downloads and indexes](MOD_CATALOG.md): catalog format, custom sources and verified downloads.
- [In-game mod manager](INGAME_MOD_MANAGER.md): supported live controls and restart requirements.
- [LAN multiplayer](LAN_MULTIPLAYER.md): hosting, joining and current test coverage.
- [Modding Freedom Unbound](MODDING.md): how to create compatible mod packages.
- [Release notes](RELEASE_NOTES.md) and [What's new](WHATSNEW.md): Alpha 0.1.5 changes.
- [Disclosures and legal disclaimer](DISCLAIMERS.md).

## Credits

- **Team Wyvern Works / DuduX42:** Freedom Unbound and its launcher.
- **PortableKit and PSPRecomp contributors:** the recompilation framework and runtime that make this project possible. See [PortableKit](https://github.com/TeamGDB/PortableKit).
- **Capcom:** the original Monster Hunter Freedom Unite and Monster Hunter series.
- **Third-party library contributors:** SDL, FFmpeg, Dear ImGui, Vulkan Loader, miniz, stb, tiny-AES and xxHash. Their license notices are included with the Windows package.
- **Our playtesters and community:** testing, bug reports and feedback.

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
