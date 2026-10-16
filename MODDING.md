# Modding Freedom Unbound

This source tree supports **texture**, **text**, and registered native
**overlay**, **camera** and **qol** providers. Audio mods remain unsupported.
A native provider ZIP is accepted only when its ID has a provider compiled into this
build; the original CWCheat damage-number ZIP is not a launcher mod.
See [native overlay support](NATIVE_OVERLAY_SUPPORT.md) for the authoring API
and the damage-event feed. Existing released binaries do not gain these hooks
until rebuilt.

Current native providers include `damage-numbers`, `gathering-nodes` and
`monster-hp`, plus the quest-only `modern-camera` camera provider. See
[Modern Camera guide](MODERN_CAMERA_MOD_GUIDE.md) for installation, controls and
verification. See [Monster HP guide](MONSTER_HP_MOD_GUIDE.md) for the latter.
Modern Camera 0.2.0 also exposes `target_camera`: ZL/LT/L2 snaps to the
selected large monster and ZR/RT/R2 cycles targets in the current quest area.
It marks the selected monster's HP bar when Monster HP is enabled, otherwise
showing its own target icon and name.
Gathering Nodes exposes `gathering_nodes` and `gathering_repeat` alongside `overlay_draw`; see
[Gathering Nodes guide](GATHERING_NODES_MOD_GUIDE.md) for its behavior and package.

`shared-guild-progression` is a QoL provider exposing `shared_guild_progression`.
It gives joined players the normal completion path for Gathering Hall urgent
quests; ordinary Hall quests already share completion. Each participant needs
the mod enabled locally. See the [Shared Progression guide](SHARED_GUILD_PROGRESSION_MOD_GUIDE.md).

`key-quests` exposes `key_quests` and draws a gold star beside native key quest
names in Village and Hall quest pages. See the [Flag Key Quests guide](KEY_QUESTS_MOD_GUIDE.md).

Players install a mod by dropping its `.zip` onto the launcher's **Mods**
page. The launcher shows the mod's details, the player chooses Install, and
then switches the mod on or off from the list. Players never unzip anything.

## Zip layout

Put everything at the root of the zip, not inside an extra folder:

```
my-texture-mod.zip            my-text-mod.zip
  mod.json                      mod.json
  textures.ini                  text\0123.txt ...
  (images textures.ini uses)    text\host.txt
                                font\font.png
                                font\map.txt
```

`textures.ini` and the image paths inside it work like a PPSSPP texture pack,
relative to the folder `textures.ini` is in. If it isn't at the root, the
launcher uses the shallowest folder that has one, so older zips still install.

- Files in the zip may not point outside it (paths with `..` or a drive letter
  are refused).
- Don't include game files from the disc, decrypted data or anything from a
  save.

## mod.json

Every mod has a `mod.json` at the top of its zip (or inside the zip's one top
folder).

```json
{
  "id": "luis-ptbr",
  "name": "Português (Brasil)",
  "version": "1.0.0",
  "author": "Luis Eduardo",
  "type": "text",
  "api_version": 1,
  "language": "pt-BR",
  "description": "Brazilian Portuguese text and font.",
  "requires": [],
  "conflicts": [],
  "affects_save": false,
  "load_order": 0
}
```

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | Lower-case letters, digits, `-`, `_`, `.`. Keep it the same in every version: a zip with the same id replaces the installed copy and keeps its on/off state. |
| `name` | yes | Shown on the Mods page. |
| `version` | yes | Free text, e.g. `1.0.0`. |
| `author` | yes | |
| `type` | yes | `texture`, `text`, `camera`, `qol`, `overlay` or `audio`. |
| `api_version` | yes | Mod API the mod is written for. This build is **1**. A higher number is refused. |
| `description` | no | Shown when installing and in the footer. |
| `language` | text mods | BCP 47 tag, e.g. `pt-BR`. Required for `text`. |
| `requires` | no | Hooks the mod needs. This build has `texture_pack`, `text_hook`, `font`; registered native camera providers expose their named hooks; overlay providers also expose `overlay_draw` and their own named hooks. A missing hook refuses the mod. |
| `conflicts` | no | Ids of mods that shouldn't be on at the same time. Shown as a warning, not enforced. |
| `affects_save` | no | `true` if the mod changes save data. Default `false`. |
| `load_order` | no | Number. Where the mod goes in the load order when it's switched on (lower loads first). Default `0`. |
| `game_id` | no | Disc id the mod is for, e.g. `ULUS10391` (USA). A mod for another release is refused. |

Older texture packs keep working: a `mod.json` that says `"type": "textures"`,
has no `type`, has no `api_version`, or no `mod.json` at all with a
`textures.ini` inside, loads as a texture mod. New mods should fill in every
required field; for anything other than `texture`, missing ones refuse the mod.

## Folder layout

Each mod is unpacked to `<data>\mods\<id>\`, with `mod.json` at the top.
Content goes in folders named after what they hold:

```
<data>\mods\
  enabled.txt            mods that are on, one id per line, in load order
  <id>\
    mod.json
    settings.json        the mod's own settings (never in the save)
    textures\            texture mods: textures.ini + images
    text\                text mods: game tables and host.txt
    font\                text mods: font.png, map.txt
```

A mod's own settings live in `<data>\mods\<id>\settings.json`, never in the
save. `enabled.txt` is written by the launcher; it is safe to edit by hand while
the launcher is closed.

## Text mods

### Game tables: `text\<entry>.txt`

- One file per game text table, named with a four-digit entry number, e.g.
  `text\0019.txt`. UTF-8 **without** a BOM.
- One string per line: `<section>.<string>=<text>`. The game's text table has
  two levels, e.g. `0.12=Sim`.
- `#` starts a comment.
- Escapes: `\n` new line, `\\` backslash, `\{` a literal `{`, `{c:N}` colour,
  `{v:N}` variable, `{x:HH}` raw byte.
- Game codes such as `~B01` (a button icon) pass through unchanged.
- A missing line keeps the English text.
- A letter with no glyph shows as `?` and is logged once.
- Use the real letters (`ã`, `ç`...): the game reads UTF-8.

**Current limitation:** accented letters in these game `text\` files currently
show as `?` until the glyph-selection routine is hooked. Unaccented translated
text works. This does **not** apply to `host.txt`: those strings use the host
TrueType font, so accents already display correctly in save dialogs and the
launcher.

Known entries:

| Entry | File | Holds |
|---|---|---|
| 19 | `text\0019.txt` | Title menu, Yes/No, Press Start, Memory Stick messages |
| TBD | | Village menu, item help text (pending a second logging run) |

```
# 0019.txt
0.0=Novo jogo
0.12=Sim
0.13=Não
```

### Host strings: `text\host.txt`

Strings that come from our own code rather than the game's data (save and
Memory Stick dialogs, the launcher's save summary). Owned by FU Saves. Same
format as the game tables: UTF-8 without a BOM, the same escapes (including
`\{`) and `#` comments. Keys are names instead of numbers, and `{0}`, `{1}`...
placeholders are filled in by our code:

```
save.confirm=Salvar o jogo?
launcher.save.slot=Espaço {0}
```

### Font: `font\font.png` + `font\map.txt`

- The loader uses the **first enabled text mod**'s `font\font.png`. It must
  be **256x256**.
- Same 12x7 grid of 20x22 cells as the game's font. The loader keeps every
  20x22 cell that isn't fully transparent and pastes those over the game's
  font atlas. It **never touches row 7** (y 154-175), the game's runtime
  strip — leave that strip fully transparent.
- Recommended layout for new letters: the game's characters stay in cells
  0-83; cells 84-95 are reserved; new letters go in cells **96-119**,
  starting at y=176. Opaque cells outside that range are still pasted if you
  draw them.
- One PNG covers every colour: colour comes from vertex colour. Draw letters
  in plain white and shade only with alpha. The game uses 16 alpha levels, so
  finer shading is rounded.
- Fixed width: there is no `width=` field.
- No `textures.ini` in `font\`.
- The font is applied before the texture mod and wins where they clash.

`font\map.txt` maps each letter to its cell, one per line. The 24 letters, in
cell order: á à â ã é ê í ó ô õ ú ç Á À Â Ã É Ê Í Ó Ô Õ Ú Ç

```
á=U+00E1 cell=96
à=U+00E0 cell=97
...
Ç=U+00C7 cell=119
```

## Dumping textures

To make a texture mod you need the game's original textures. On the
launcher's **Mods** page, switch on **Dump textures** (before starting the
game). From the next start, every new texture the game draws is saved to
`<data>\TextureDump` as a hash-named PNG with a `textures.ini`.

- The texture mod is skipped while dumping (it changes the dump's file names);
  your enabled mods are left as they are.
- The frame rate is lower while dumping.
- Switch it off again when you're done; it also applies on the next start.
- Dumps are original game artwork. **Never commit them** or ship them in a
  mod. `TextureDump/` is in `.gitignore`.
## Rules the launcher enforces

- Several mods can be on at once.
- **One texture mod** at a time, and **one text mod per language**. Switching a
  new one on switches the old one off, and the launcher says so.
- A text mod's font replaces **only the font textures**, layered on top of
  whichever texture mod is on. The two don't switch each other off.
- Any other overlap is settled by load order: mods load top to bottom in
  `enabled.txt`, and a later one wins. On the Mods page, **Move up / Move down**
  changes the order. `load_order` only picks where a mod is placed when it is
  first switched on.
- `conflicts` is shown as a warning when both mods are on.
- A mod is refused if its `type` isn't supported in this build, its
  `api_version` is newer than the build's, it `requires` a hook the build
  doesn't have, or (text mods) it has no `language` or no `text\` folder with
  at least one file.
- A text mod needs a `text\` folder holding at least one file; one with only
  `host.txt` is valid. `font\` is optional, and a mod with only a `font\`
  folder is refused.
- **`affects_save: true`**: the launcher shows a notice. FU Saves writes
  `mods-used.txt` next to the save, listing the mods that were on, and backs up
  the save (FU Saves' `.backup`) only when that list changes, so the backup
  folder doesn't fill up with a copy on every launch.

## Mods page

- Mods are grouped by type, each showing its version, language and status
  (On with its place in the load order, Off, or Can't be used and why).
- Conflict and save warnings show under the mod.
- Enabled mods can be moved up or down; the order is saved in `enabled.txt`.
- Drag-and-drop `.zip` install, with a preview before installing.

## For the launcher's own code

`host/mod_manager.hpp`, namespace `portablekit::fu::mods`:

- `enabled_text_mods()` → enabled text mods in load order, one per language:
  `id`, `language`, `folder`, `text`, `font`, `host` paths (`font` and `host` may not exist). For the string hook,
  the font loader and `host.txt`.
- `enabled()` → every enabled, usable mod in load order (`info.affects_save`
  tells FU Saves what goes in `mods-used.txt`).
- `kApiVersion` → the build's mod API version.
