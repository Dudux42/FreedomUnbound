# Freedom Unbound — Alpha 0.1.4

Created by **Team Wyvern Works**

A Windows native recompilation of **Monster Hunter Freedom Unite**, built on [PortableKit](https://github.com/TeamGDB/PortableKit).

Play Freedom Unite on PC with a dedicated launcher, configurable controls, save management and optional mods. You need your own supported USA copy of the game; game files and saves are not included.

## Current stage — Alpha 0.1.4

**Playable alpha.** Various Village Elder quests have been tested through **★3**, including progression, hunting, saving and reloading. The full game is expected to be playable, but a complete playthrough has not yet been verified.

Current features:

- Native PC gameplay at the original **30 FPS**, with optional experimental interpolation.
- A dedicated launcher with English and Brazilian Portuguese interface options.
- Three save cards with rotatable equipped hunters, equipment details, rank, Zenny, Pokke/Guild Points and play time.
- A **Load** button for each occupied card to start that hunter.
- Automatic and manual save backups, restore, and compatible PSP save import/export.
- Configurable keyboard, mouse and gamepad controls.
- Optional texture, text and supported gameplay mods.
- Optional cheats for progression testing.
- Screenshots, update checks and a built-in bug report tool.

**Known issues:** experimental interpolation can distort the background inside the hunter's house. Use 30 FPS if this happens. Brief loading hitches may occur. LAN connections and player movement are working; full multiplayer quest completion, rewards and save/reload validation remain pending. Internet play, cross-emulator compatibility and broader hardware coverage are unverified. The **Fill** display option is not yet available.

## What's new

- Verified mod downloads, with progress, cancellation and installed-version labels.
- **Freedom Unbound Official Mod Page** is included; add other public GitHub
  indexes and expand/collapse each group to organize its mods.
- **F9** opens the in-game mod manager; **F10** toggles an installed Freedom Unbound HD pack.
- Pre-game LAN Host/Join controls, alongside the equipped-hunter preview cards
  and per-hunter loading from earlier builds.

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

| Mod support | What it does |
| --- | --- |
| Texture packs | Replace supported game textures, including HD artwork. Enable one texture pack at a time. |
| Text mods | Replace supported game text and fonts. The launcher's language setting is separate from the game's translation. |
| Gameplay and HUD mods | Add supported features such as damage numbers, gathering indicators, monster health, camera controls and key-quest markers. These require a compatible launcher version. |

Browse and download mods directly from the launcher. The **Freedom Unbound Official Mod Page** is included; you can add custom public GitHub indexes and expand or collapse each group. Downloads are checked against the index's size and SHA-256 before installation review.

To install a ZIP manually, **do not extract it**. Drop it onto the launcher's **Mods** page, review the details, choose **Install**, then turn it on.

Press **F9** to manage supported mods in-game. **F10** toggles an installed Freedom Unbound HD texture pack. New installs remain disabled until you enable them. See [Mod downloads and indexes](MOD_CATALOG.md) and [In-game mod manager](INGAME_MOD_MANAGER.md).

Mods are downloaded separately. Check each mod's guide for launcher requirements, controls and known limitations. Some mods change saved progress.

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
| Aspect ratio | Original preserves the PSP picture's proportions. Stretch fills the window by stretching it. Fill is not yet available. |
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

**Useful controls:** Esc or L3+R3 opens the in-game menu; F3 toggles performance information; F12 takes a screenshot. The launcher can open your screenshots folder.

Use **Saves** to manage backups, restore or import/export a compatible save. Keep a separate copy of your original PSP save and follow any restart prompts.

Optional **Cheats** include unlimited health/stamina, one-hit kills, maximum Zenny, item-box stack multiplication, free crafting for available recipes and L+R quest completion. Some cheats require a restart after disabling. Money, items, crafted equipment and saved progress are not undone when a cheat is turned off.

To report a problem, use **Report a bug** in the launcher and share the report through the server's bug-report channel. Include what happened and how to reproduce it. The report excludes your saves, save keys and game files.

## Project documentation

- [Freedom Unbound Mods](https://github.com/Dudux42/FreedomUnboundmods): my FU mods, downloads and player guides.
- [Mod downloads and indexes](MOD_CATALOG.md): catalog format, custom sources and verified downloads.
- [In-game mod manager](INGAME_MOD_MANAGER.md): supported live controls and restart requirements.
- [LAN multiplayer](LAN_MULTIPLAYER.md): hosting, joining and current test coverage.
- [Release notes](RELEASE_NOTES.md) and [What's new](WHATSNEW.md): Alpha 0.1.4 changes.
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
