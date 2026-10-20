"""Capture health after the USA monster HP store and maximum clamp."""
from pathlib import Path
import argparse

def lifecycle(source, header):
    for address in (0x0886F0BC, 0x0886F0F8):
        statement = 'aot_mem.aot_store8(ctx.gpr[3] + static_cast<std::uint32_t>(46), static_cast<std::uint8_t>(ctx.gpr[4]));'
        expected = f'L_{address:08X}:\n    {statement}'
        if source.count(expected) != 1:
            raise ValueError('Unsupported or already instrumented USA quest transition')
        source = source.replace(expected, f'L_{address:08X}:\n    portablekit::fu::monster_hp::clear();\n    {statement}')
    return f'#include "{header.resolve().as_posix()}"\n' + source

def instrument(source, header):
    expected = 'L_09AC7AF4:\n    ctx.gpr[2] = (0u + static_cast<std::uint32_t>(9));'
    if source.count(expected) != 1:
        raise ValueError('Unsupported or already instrumented USA monster HP routine')
    return f'#include "{header.resolve().as_posix()}"\n' + source.replace(expected,
        'L_09AC7AF4:\n    portablekit::fu::monster_hp::capture(ctx, aot_mem);\n    ctx.gpr[2] = (0u + static_cast<std::uint32_t>(9));')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ('source', 'output', 'header'):
        parser.add_argument(field, type=Path)
    parser.add_argument('--lifecycle', action='store_true')
    args = parser.parse_args()
    result = (lifecycle if args.lifecycle else instrument)(args.source.read_text(encoding='utf-8'), args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != result:
        args.output.write_text(result, encoding='utf-8')
