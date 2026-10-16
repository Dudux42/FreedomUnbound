# Standalone Freedom Unbound Save Editor

Created 14 October 2026; Save workflow updated 15 October 2026. This is a separate Windows application,
**FreedomUnboundSaveEditor.exe**. It does not launch the game or change launcher
settings. Close FreedomUniteNative before editing; the editor refuses writes
while that process is running.

## Use

1. Open the executable. It suggests the normal Freedom Unbound installation
   when one exists. **Save profile** defaults to `FreedomUnite (current game)`.
   The older `FreedomUnite-Test` profile remains selectable explicitly; both can
   contain the same hunter names. Check the displayed path and Save target. Otherwise choose
   **Open save...** and select
   `ms0/PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN`.
2. **Game data...** selects the installed `EBOOT.ELF` prepared from your USA
   game image. It is detected automatically when beside the selected `ms0`.
   The ISO location comes from that installation's `settings.ini`; a file
   chooser appears if it cannot be found. Game files are read only.
3. Choose the hunter slot. Enter Zenny and Pokke Points. Both accept whole
   numbers from 0 to 9,999,999. Currency edits remain pending until **Save**.
4. Search for an item by name or ID, choose it, enter its quantity and press
   **Add to item box** to queue the addition. Repeat for more items if needed.
5. Click the dedicated **Save** button at the bottom right. It writes all pending
   currency and item changes together, creates one backup, and reports success
   only after verifying and reopening the save. Switching hunters preserves each
   hunter's pending edits. Closing or opening another save asks before discarding
   unsaved changes. Selecting an item alone does not add it; use **Add to item box**.

Each added stack contains at most 99. Larger quantities split across the first
empty slots in index order, without overwriting or merging existing stacks.
Existing stacks above 99 (including stacks produced by cheats) are preserved
at their original quantities and do not prevent opening or editing the save.
The game exposes 800 slots initially, 900 with one storage Guide, and 1,000 with
both. Locked pages are never filled. If there is insufficient space, the entire
addition is refused; no partial quantity or currency changes are written.

Every successful Save retains an exact copy of the original save folder under
`<data>/SaveEditorBackups/<timestamp>/ULUS10391/`. These editor backups are kept
separately from the launcher's five rotating backups and are not automatically
deleted. **Open last backup** shows the latest one. To undo an edit, close the
game/editor and restore that backup folder through the launcher's save-import
function or copy its contents back to the original save folder.

Only the chosen hunter's requested currency fields and empty item records are
modified. Quest progress, rank, equipment, other hunters and item-history flags
are preserved. Obtaining an item through the editor does not set separate
progression/unlock flags. Invalid saves, unsupported executable/region, malformed
inventory, invalid quantities and externally changed saves are refused.

## Implementation and verification

- `host/save_editor_main.cpp`: native Windows controls, save/game selection,
  searchable catalogue, hunter selection, currency and item actions.
- `host/save_editor.cpp`: installed-game metadata, field validation, staged save
  writes, exact original backups, stale-session detection and folder rollback.
- `host/save_preview.cpp`: shared inner encoding/decoding with salted SHA-1.
  The original read-only preview API is retained.
- PortableKit's existing save crypto handles outer PSP encryption and file hashes.
  Original media files and additional PARAM.SFO fields are preserved.
- The inner save table, hunter-save key and archive substitution table are read
  from the exact supported installed ELF. Item names come from the owner's
  ULUS10391 ISO, DATA.BIN entry 19 section 1. No game tables, per-game key,
  item-name database or save files are distributed with the editor.

Measured save fields, adding `hunter_index * 0x6B100`: Zenny `0x6A250`, Pokke
Points `0x6A248`, and item-box records `0x3F88` (1,000 four-byte ID/quantity
records). The Guide ownership bitset starts at `0x6B7B4`; IDs 829/830 control
the extra pages. These were checked against the USA currency update, storage
limit and ownership routines at `0x0884FBA4`, `0x0884FC0C`, `0x08850E58`,
`0x08850EC4`, and `0x08857C98`. The hunter-save key initializer is separate from
downloaded-content save initialization; its values are never embedded or logged.

Synthetic tests cover all three hunters, codec round trips and corruption,
preservation of existing oversized stacks,
exact allowed byte changes, full/partially full boxes, locked/expanded pages,
plain and encrypted disk transactions, backups, extra SFO fields, refreshed
previews, external changes and backup failures. Local validation evidence is
under `build/windows/investigation/save-editor/`, with the full regression result
in `build/windows/save-editor-tests.log`.

Build the standalone targets after the normal Windows CMake configuration:

```powershell
cmake --build build/windows --target FreedomUnboundSaveEditor mhfu_save_editor_tests --parallel 2
ctest --test-dir build/windows -R mhfu_save_editor_tests --output-on-failure
```

The GUI uses Windows controls and the existing icon. It needs no Python, Vulkan,
SDL or FFmpeg. Like the native game, it uses the Microsoft Visual C++ x64 runtime.
Native game acceptance is performed only on copied saves; broader live usage
remains a player check, not a claim that every inventory combination was tested.

Local handoff: `build/windows/save-editor/FreedomUnboundSaveEditor.exe`, with
instructions, dependency licenses and a source/binary hash manifest beside it.
All **43/43** regression targets passed after the Save workflow update. Native control diagnostics saved
100,001 Zenny, 2,222 Pokke Points and 17 Mega Potions to a separate GUI fixture.
A second edited fixture (543,210 Zenny, 12,345 Pokke Points and 120 added Potions)
loaded successfully in the native game and reached village gameplay. Both native
checks were bounded; the captured run completed in 31.02 seconds, with runtime
window-close exit 4, native 30 simulation, unthrottled navigation and audio off.
The editor's original source save-folder hashes remained unchanged. Actual
player saves were not edited for these checks.

The standalone build completed. A separate attempt to build every game overlay
was interrupted by an unrelated stage200 DLL being in use; no running user
process was terminated, no frozen game package was replaced, and the standalone
targets and regression tests were completed independently.

On 15 October, native GUI diagnostics selected Invoker in a copy of the actual
two-character save, queued 17 Mega Potions and edited both currency fields.
The file stayed unchanged before Save; switching hunters retained pending edits.
Save then wrote the batch, and a fresh Session reopened it with the expected
values. Luis remained unchanged. The original two-character save hashes were
verified unchanged. Synthetic plain/encrypted batch tests also check that an
invalid later item prevents all pending changes from being written.

Pokke Points investigation, 15 October: the gameplay build is configured with
MHFU_TEST_DATA=False and uses FreedomUnite. Both installed profiles now contain
Luis and Invoker; the editor's prior automatic Test-profile preference selected
the older copy. The profile dropdown and normal-profile default correct that
mismatch without copying over either save. A copied gameplay save reported
335 points for Invoker; a Save-button diagnostic changed it to 2,222 and reopened
the file successfully. Executing the installed USA ELF's original 0884FC0C
point-update routine in isolated guest memory produced 2,232 for +10 and 2,212
for -10. This checks the original award/spend routine, not an entire live quest.
No point-freeze behavior was reproduced. Current game data and backups are kept
separate. Test-profile original hashes remained unchanged, and the normal
profile was opened only for a read-only startup capture.

Latest full-suite run after the profile correction: 42/43 passed. The save-editor
target passed; the unrelated mhfu_monster_hp_tests executable failed with
0xc0000409. HP/icon work was cancelled by the owner, so it was left untouched.

Direct in-game verification, 15 October: the owner's next successful edit
wrote 99,335 points to Invoker in the current FreedomUnite profile. A fresh
copy of that file was loaded with the exact build/windows/bin native executable
targeted by the desktop shortcut. The actual Status screen (menu page 2, first
entry) displayed Invoker, PokkePt 99335pts and GuildPt 1420. Evidence:
build/windows/investigation/save-editor/points-display/desktop-run/captures/status.bmp.
That bounded run completed in 30.17 seconds, exit 4, using an isolated save,
no mods, audio disabled and native 30 simulation. The previous package check
also showed an edited 2,222 Pokke Points in-game. This establishes visible
Pokke Point loading, beyond the earlier codec/field and award-routine checks.
No fresh Pokke Points field fix was needed in this verification.

Save now displays a confirmation with the full file path and saved currency
values for each hunter. Save failures also appear in a dialog instead of only
in the bottom status text. The running-game error correctly requires closing
the launcher too, since the native launcher and game share one process.
