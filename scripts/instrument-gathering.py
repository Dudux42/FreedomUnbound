"""Add the guarded gathering HUD callback to a build copy of USA main unit 16."""
from pathlib import Path
import argparse

def instrument_input(source, header):
    statement = 'aot_mem.aot_store32(ctx.gpr[4] + static_cast<std::uint32_t>(1156), ctx.gpr[5]);'
    expected = 'L_088D61FC:\n    ' + statement
    if source.count(expected) != 1:
        raise ValueError('Unsupported or already instrumented USA input copy')
    return f'#include "{header.resolve().as_posix()}"\n' + source.replace(expected,
        'L_088D61FC:\n    portablekit::fu::gathering_nodes::repeat_input(ctx, aot_mem);\n    ' + statement)

def instrument(source, header):
    expected = 'L_08845CE4:\n    ctx.gpr[2] = (2196u << 16u);'
    if source.count(expected) != 1:
        raise ValueError('Unsupported or already instrumented USA gathering HUD')
    replacement = 'L_08845CE4:\n    portablekit::fu::monster_hp::refresh(ctx, aot_mem);\n    portablekit::fu::gathering_nodes::update(ctx, aot_mem);\n    ctx.gpr[2] = (2196u << 16u);'
    return f'#include "{header.resolve().as_posix()}"\n#include "{(header.parent / "monster_hp.hpp").resolve().as_posix()}"\n' + source.replace(expected, replacement)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('header', type=Path)
    parser.add_argument('--input', action='store_true')
    args = parser.parse_args()
    result = (instrument_input if args.input else instrument)(args.source.read_text(encoding='utf-8'), args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != result:
        args.output.write_text(result, encoding='utf-8')
