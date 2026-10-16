"""Observe the supported USA text draw calls in a build copy, without changing game data."""
from pathlib import Path
import argparse

POINTS = {0x08891474: 864, 0x088915B0: 896, 0x0889198C: 32, 0x08891A28: 32}

def instrument(source, header):
    for address, size in POINTS.items():
        statement = f'ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-{size}));'
        label = f'L_{address:08X}:\n'
        expected = label + '    ' + statement
        if source.count(label) != 1 or source.count(expected) != 1 or 'key_quests::capture' in source:
            # Check already-instrumented input before replacing any point below.
            raise ValueError('Unsupported or already instrumented quest-name renderer')
    for address, size in POINTS.items():
        statement = f'ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-{size}));'
        expected = f'L_{address:08X}:\n    {statement}'
        source = source.replace(expected, f'L_{address:08X}:\n    portablekit::fu::key_quests::capture(ctx, aot_mem, 0x{address:08X}u);\n    {statement}')
    return f'#include "{header.resolve().as_posix()}"\n' + source

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'output', 'header'): parser.add_argument(name, type=Path)
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding='utf-8'), args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != result:
        args.output.write_text(result, encoding='utf-8')
