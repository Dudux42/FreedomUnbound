"""Add text-mod hooks to build-directory copies of USA units (see host/fu_text_mod.hpp).

Same approach as instrument-cheats.py: a call is inserted after an exact label
whose translated instruction is checked first. No game code is stored here.
"""
import argparse
from pathlib import Path

POINTS = {
    # DATA.BIN worker, after read + decrypt + verify, before completion.
    "main18": {0x0884E648: ("ctx.gpr[2] = (3u << 16u);",
                            "    portablekit::fu::text::entry_loaded(ctx, aot_mem);\n")},
    # Code point -> glyph code (table 0x08910A88, '?' if unmapped).
    "main04": {0x088165FC: ("if (ctx.gpr[5] != 0u) {",
                            "    if (portablekit::fu::text::map_glyph(ctx, aot_mem)) goto L_08816630;\n")},
    # Glyph cache lookup (a0 = text object, a1 = glyph code): pin mod glyphs.
    "main35": {0x08893198: ("ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-16));",
                            "    portablekit::fu::text::glyph_cache_lookup(ctx, aot_mem);\n")},
}

def instrument(source, kind, header):
    for point, (statement, call) in POINTS[kind].items():
        label = f"L_{point:08X}:\n"
        expected = label + "    " + statement
        if source.count(label) != 1 or source.count(expected) != 1 or "fu::text::" in source:
            raise ValueError(f"Unsupported or already instrumented {kind} at {point:08X}")
        source = source.replace(expected, label + call + "    " + statement)
    return f'#include "{header.resolve().as_posix()}"\n' + source

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
