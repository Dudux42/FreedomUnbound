# Gathering Nodes: native launcher mod

Author: DuduX42. Version: 0.2.3.
Target: modified Freedom Unbound Windows build, USA ULUS10391, mod API 1.

## Install and behavior

Drop `gathering-nodes-0.2.3.zip` onto the launcher's Mods page, install it and
switch Gathering Nodes on. The desktop shortcut uses the rebuilt native app.
The ZIP selects a compiled provider; it cannot add support to an older build.

A forest-green hexagonal question mark appears above a nearby usable gathering
spot when the hunter is sheathed. Its color follows the Freedom Unbound emblem.
The red Circle sub-badge is removed. The marker follows the hunter/camera and
stays clear of the head and centered item-notification band. There is no text
prompt. It disappears with a drawn weapon, out of range, or at zero remaining
node uses. For overlapping radii, the closest usable node supplies the icon.

Hold the PSP Circle action (including its configured keyboard/controller
binding) to gather, mine or catch bugs repeatedly. Ordinary gathering uses
the native interaction dispatcher, including its crouched follow-up states;
it does not restart the current animation. Mining and bugs require a
matching tool with positive quantity in the pouch. Release Circle to stop.
Depletion or leaving the original spot cancels the held session; it does not
silently switch to an overlapping node. A press held through weapon draw and
sheathing must be released before gathering can resume.

The original game awards items, consumes tools and decrements remaining uses.
The mod does not give unlimited items/tools or directly modify save files.
Disable it from Mods to remove both the indicator and the repeated action.

## Supplied archive and provenance

The reference archive contained CWCheat files `ULUS-10391.TXT` and
`ULJM-05500.TXT` with Gathering Node Indicators groups. No source license,
manifest or author documentation accompanied them. They were analyzed as
reference data, not executed or treated as instructions from the user.
Original cheats, game code and game artwork are not redistributed.

Reference SHA-256: `F8B8618B8976832D7D095F7758A172A62B4C6F5C178FA058984FBBBDCDAD3895`.

## Native implementation

The implementation is independently written C++ for the measured USA game.
`scripts/instrument-gathering.py` inserts a guarded HUD callback at 0x08845CE4
into a build-directory copy. It also adds an input callback at 0x088D61FC,
composed after the existing main-unit-52 cheat instrumentation.
Original generated game sources remain unchanged.
The callback validates that HUD instruction and the quest routine 0x09A6F490.

The hunter pointer is u32[0x099998F4]. The current zone descriptor is resolved
from state u32[0x089CC558], map u32[0x08A62D9C], area byte state+0x6AF10 and
mapping table 0x09CE4808. Map+0x60 supplies offsets and map+0x50 the node data.
Pointers, lengths, finite coordinates and radius bounds are checked. Lists use
24-byte records and an x=-1 terminator, bounded to 256 records per scan.
Node xyz/radius are floats at +0/+4/+8/+12, ID is u16+16, remaining uses u16+18
and type is u16+20. Zero-use/invalid records do not produce an indicator.
The game's harvest routine checks +18 at 0x09AAAC10 and decrements or clears
it at 0x09AAAD44 through 0x09AAAD78.

Type 3 is mining, type 4 bug catching. Pouch entries are four bytes across
25 slots; pickaxes are 0x90 through 0x92 or 0x4C9, bug nets 0x94 through 0x96
or 0x4CA. Tool quantity must be positive. Other normal gathering spots use the
general gathering action.

Weapon visibility uses the persistent drawn bit at hunter+0x410, mask 0x08.
USA weapon routines set this bit, including 0x09A71B68. Earlier live bow
samples read flags 0x201 sheathed versus 0x209 drawn. The general action group
at +0x298 is insufficient by itself: drawn melee idle also uses group zero.
Nonzero action groups are excluded as unsupported interaction states.

The held mask is u32[0x08A62D88], Circle bit 0x2000. While a valid held
gathering session is active, the input callback adds Circle to the local
hunter's fresh-press mask (+0x484), at the instruction that copies input into
the actor. It runs before the original menu/action filters. It requires the
instruction signature 0xAC850484 and the local hunter argument. Updating the
camera/UI input copy instead did not repeat gathering in a native playtest.
Global input, camera/UI input, held/released masks and other buttons remain
unchanged. Ordinary gathering never forces +0x299 or resets animation
counters: native eligibility, transitions and loot logic remain in control.
This also permits the game's crouched follow-up states rather than requiring
standing idle for each request.

The established tool shortcut starts 0x51 mining or 0x52 bugs only when
+0x299 is zero, sets the tool at +0x694 and clears +0x1D5/+0x1D6. These
writes run on the guest thread. The repeated-node identity and a release latch
prevent an interrupted/combat press from restarting at another node.
The input callback rechecks quest phase (2/3), menu state, weapon state,
remaining uses and the original node radius before injecting input. The mod
never edits remaining uses, item rewards or tool quantities itself.

The renderer receives copied status under a mutex. The approximate head point
is projected with the native camera/projection and drawn as an authored vector
hexagon. No guest pointer is dereferenced while drawing. Placement preserves
the established head clearance and item-notification exclusion rectangle.

## Build and verification

```powershell
cmake --build build/windows --target FreedomUniteNative mhfu_gathering_nodes_tests --parallel 2
ctest --test-dir build/windows -R mhfu_gathering --output-on-failure
python scripts/package-gathering-nodes.py
```

The ZIP contains mod.json and this guide. Its manifest requires overlay_draw
and gathering_nodes plus gathering_repeat, preventing installation into older
builds without the input fix. Author is DuduX42 with affects_save=false.

Regressions cover node type/tool/action matching, boundaries, depletion,
overlap, drawn melee idle, draw/aim groups, hiding existing status, blocked tool
writes, and restoration after sheathing. Held-input cases cover all three
actions, ordinary follow-up states, local-only input injection, release, depletion,
menu/reward transitions and leaving/reentering the radius.

Earlier version checks reached the village and Snowy Mountains camp and used
labeled marker samples to verify projection/placement. Those samples are not
proof of live harvest results. Full gathering/tool-consumption coverage, all
maps and multiplayer remain playtest limits.

Developer-only MHFU_GATHERING_PREVIEW=1 draws a labeled marker fixture using
the same weapon visibility guard, without changing node state or gameplay.
MHFU_GATHERING_TRACE=1 logs node/weapon/repeat diagnostics. Neither variable
is required for ordinary play or added to the desktop shortcut.

## Earlier icon verification

The native application rebuilt successfully and all eight selected regression
targets passed. A bounded 960x544 in-game run at Snowy Mountains camp confirmed
the green marker without a Circle badge, its disappearance while the real melee
weapon was drawn, and restoration after sheathing. The trace read 0x201, then
0x209 while drawn, then 0x201. The run exited normally at frame 4700, and copied
save hashes were unchanged. Evidence is under
`build/windows/gathering-green-fixed-live/`.

This earlier live check used a labeled marker fixture and did not establish
that continuous gathering worked. Version 0.2.3 separately checks native
harvest results rather than relying only on synthetic action-state tests.

## 0.2.3 continuous gathering verification

The native executable rebuilt successfully and all nine selected regression
targets passed, including gathering, camera, monster HP, overlay registration,
text instrumentation and cheat instrumentation.

A bounded native-only playtest used a copied save and an original ordinary
node aliased into the empty Snowy Mountains camp. Test setup positioned that
node beside the hunter and initialized three guaranteed uses. Its native loot
ID/type stayed unchanged; the harness did not simulate item awards or decrement
uses. One uninterrupted Circle hold produced native uses 3 -> 2 -> 1 -> 0.
The pouch gained three Honey (item 0xA1). After depletion the hunter returned
to idle, the injected pressed bit cleared while Circle was still physically
held, and the icon disappeared. Screenshots show the green icon above the
unobstructed Honey notification during gathering and no icon afterward.

The scripted run ended at frame 5500 ("Runtime stopped: window closed").
Copied save hashes were unchanged. Evidence is under
`build/windows/gathering-native-repeat-live4/`, including native-fixture.json
and the gathering-a/gathering-b screenshots. Earlier comparison runs completed
only one harvest, confirming that camera/UI input alone was insufficient.

This verifies ordinary gathering's full native harvest cycle. Mining, bugs,
all-map coverage and multiplayer remain playtest limits for this revision;
their tool selection and interruption guards have regression coverage.
