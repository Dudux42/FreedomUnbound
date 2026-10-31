"""Add the guarded camera_aim join callbacks to build copies of the USA game_task overlay."""
from pathlib import Path
import argparse
POINTS = {
    'bow': (0x09BB6F8C, '    if (ctx.gpr[16] == ctx.gpr[3]) {'),
    'bowgun': (0x09AA5248, '    ctx.gpr[5] = (0u + static_cast<std::uint32_t>(1005));'),
}
def instrument(source, header, site):
    address, statement = POINTS[site]
    expected = f'L_{address:08X}:\n{statement}'
    if 'camera_aim::' in source or source.count(expected) != 1:
        raise ValueError(f'Unsupported or already instrumented USA aim join at {address:08X}')
    source = source.replace(expected, f'L_{address:08X}:\n    portablekit::fu::camera_aim::{site}(ctx, aot_mem);\n{statement}')
    return f'#include "{header.resolve().as_posix()}"\n' + source
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'output', 'header'): parser.add_argument(name, type=Path)
    parser.add_argument('site', choices=sorted(POINTS))
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding='utf-8'), args.header, args.site)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != result:
        args.output.write_text(result, encoding='utf-8')
