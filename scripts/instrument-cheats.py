"""Add guarded native cheat hooks to build-directory copies of USA units."""
import argparse
from pathlib import Path

# Validate the exact translated instruction at each interception. No game code
# is stored here; originals remain in the ignored generated/overlays trees.
POINTS = {
    "main18": {0x0884FBBC: "ctx.gpr[5] = (ctx.gpr[6] + ctx.gpr[5]);"},
    "main19": {0x08853B84: "aot_mem.aot_store16(ctx.gpr[8] + static_cast<std::uint32_t>(0), static_cast<std::uint16_t>(ctx.gpr[2]));",
               0x08853BF4: "aot_mem.aot_store16(ctx.gpr[4] + static_cast<std::uint32_t>(2), static_cast<std::uint16_t>(ctx.gpr[2]));",
               0x08853C44: "aot_mem.aot_store16(ctx.gpr[4] + static_cast<std::uint32_t>(2), static_cast<std::uint16_t>(ctx.gpr[16]));"},
    "main22": {0x0885E214: "ctx.gpr[31] = (aot_mem.aot_load32(ctx.gpr[29] + static_cast<std::uint32_t>(44)));"},
    "main26": {0x0886E838: "ctx.gpr[8] = (ctx.gpr[4] + 0u);"},
    "lobby17": {0x09AA4A4C: "ctx.gpr[31] = (aot_mem.aot_load32(ctx.gpr[29] + static_cast<std::uint32_t>(12)));",
                0x09AA5D90: "ctx.gpr[16] = (ctx.gpr[5] + 0u);",
                0x09AA5788: "ctx.gpr[5] = (aot_mem.aot_load32(ctx.gpr[4] + static_cast<std::uint32_t>(64)));"},
    "lobby18": {0x09AA8380: "ctx.gpr[5] = (aot_mem.aot_load32(ctx.gpr[4] + static_cast<std::uint32_t>(64)));"},
    "lobby31": {0x09ADDC94: 'aot_mem.aot_store16(ctx.gpr[3] + static_cast<std::uint32_t>(12170), static_cast<std::uint16_t>(ctx.gpr[2]));',
                0x09ADE3B4: 'aot_mem.aot_store16(ctx.gpr[3] + static_cast<std::uint32_t>(12170), static_cast<std::uint16_t>(ctx.gpr[2]));',
                0x09ADE594: 'aot_mem.aot_store16(ctx.gpr[3] + static_cast<std::uint32_t>(12170), static_cast<std::uint16_t>(ctx.gpr[2]));',
                0x09ADE6B8: 'aot_mem.aot_store16(ctx.gpr[3] + static_cast<std::uint32_t>(12170), static_cast<std::uint16_t>(ctx.gpr[2]));',
                0x09ADE8D8: 'aot_mem.aot_store16(ctx.gpr[4] + static_cast<std::uint32_t>(12170), static_cast<std::uint16_t>(ctx.gpr[2]));'},
    "lobby32": {0x09AE2F88: 'aot_mem.aot_store16(ctx.gpr[6] + static_cast<std::uint32_t>(12170), static_cast<std::uint16_t>(ctx.gpr[2]));',
                0x09AE2FA0: 'aot_mem.aot_store16(ctx.gpr[2] + static_cast<std::uint32_t>(12170), static_cast<std::uint16_t>(ctx.gpr[4]));'},
    "main52": {0x088D5E30: "ctx.gpr[3] = (aot_mem.aot_load32(ctx.gpr[17] + static_cast<std::uint32_t>(1556)));",
               0x088D6594: "ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-16));"},
    "game2": {0x09A6A2D0: "ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-16));",
              0x09A6B154: "{ const bool branch_taken = ctx.gpr[3] == 0u;"},
    "game9": {0x09A85BAC: "aot_mem.aot_store16(ctx.gpr[16] + static_cast<std::uint32_t>(740), static_cast<std::uint16_t>(ctx.gpr[2]));"},
    "game26": {0x09AC7AD4: "aot_mem.aot_store16(ctx.gpr[21] + static_cast<std::uint32_t>(740), static_cast<std::uint16_t>(ctx.gpr[2]));"},
}

DEPOSITS = {0x09ADDC94: (3, 2), 0x09ADE3B4: (3, 2), 0x09ADE594: (3, 2),
            0x09ADE6B8: (3, 2), 0x09ADE8D8: (4, 2), 0x09AE2F88: (6, 2), 0x09AE2FA0: (2, 4)}

def instrument(source, kind, header):
    for point, statement in POINTS[kind].items():
        label = f"L_{point:08X}:\n"
        expected = label + "    " + statement
        if source.count(label) != 1 or source.count(expected) != 1:
            raise ValueError(f"Unsupported or already instrumented {kind} at {point:08X}")
        extra = ""
        if point in DEPOSITS:
            base, value = DEPOSITS[point]
            extra = f", ctx.gpr[{base}] + 12170u, {value}u"
        call = f"    portablekit::fu::cheat_hook(0x{point:08X}u, ctx, aot_mem{extra});\n"
        if point == 0x09AC7AD4:
            call = "    portablekit::fu::damage_events::capture(ctx, aot_mem);\n" + call
        if point == 0x0886E838:
            # Use the existing success call with its original return dispatch;
            # saved registers and the normal quest script are preserved.
            call += "    if (portablekit::fu::quest_complete_requested(ctx, aot_mem)) goto L_0886E8E0;\n"
        if point in (0x09AA5788, 0x09AA8380):
            # Registers have been saved; use the original epilogue and AOT
            # return dispatch before any inventory, fee or equipment mutation.
            epilogue = 0x09AA5B64 if point == 0x09AA5788 else 0x09AA848C
            call += f"    if (!portablekit::fu::equipment_transaction_allowed(aot_mem)) goto L_{epilogue:08X};\n"
        source = source.replace(expected, label + call + "    " + statement)
    includes = f'#include "{header.resolve().as_posix()}"\n'
    if kind == "game26":
        includes += f'#include "{(header.parent / "damage_events.hpp").resolve().as_posix()}"\n'
    return includes + source

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("kind", choices=POINTS)
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding="utf-8"), args.kind, args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != result:
        args.output.write_text(result, encoding="utf-8")
