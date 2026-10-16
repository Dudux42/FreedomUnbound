"""Instrument verified USA per-hit instructions in a build-directory copy."""
from pathlib import Path
import argparse

POINTS = {
    0x09C43558: ('ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-144));', 'begin_hit'),
    0x09C44D30: ('ctx.gpr[2] = (ctx.gpr[2] + ctx.gpr[17]);', 'commit_hit'),
}
CAMERA_POINTS = {0x0886941C: ('jump_target = ctx.gpr[31];', 'capture_camera')}

def instrument(source, header, camera=False):
    for point, (statement, callback) in (CAMERA_POINTS if camera else POINTS).items():
        expected = f'L_{point:08X}:\n    {statement}'
        if source.count(expected) != 1:
            raise ValueError(f'Unsupported or already instrumented hit routine at {point:08X}')
        extra = '    portablekit::fu::monster_hp::refresh(ctx, aot_mem);\n' if camera else ''
        source = source.replace(expected, f'L_{point:08X}:\n    portablekit::fu::damage_events::{callback}(ctx, aot_mem);\n{extra}    {statement}')
    includes = f'#include "{header.resolve().as_posix()}"\n'
    if camera:
        includes += f'#include "{(header.parent / "monster_hp.hpp").resolve().as_posix()}"\n'
    return includes + source

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('header', type=Path)
    parser.add_argument('--camera', action='store_true')
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding='utf-8'), args.header, args.camera)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != result:
        args.output.write_text(result, encoding='utf-8')
