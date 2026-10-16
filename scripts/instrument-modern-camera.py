"""Add guarded USA camera callbacks to a build copy, preserving native geometry."""
from pathlib import Path
import argparse
POINTS = {
    0x08887614: ('ctx.gpr[2] = (static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(aot_mem.aot_load16(ctx.gpr[20] + static_cast<std::uint32_t>(128))))));', 'yaw'),
    0x08887834: ('ctx.fpr[0] = std::bit_cast<float>(aot_mem.aot_load32(ctx.gpr[19] + static_cast<std::uint32_t>(512)));', 'geometry'),
}
def instrument(source, header):
    for address, (statement, callback) in POINTS.items():
        expected = f'L_{address:08X}:\n    {statement}'
        if source.count(expected) != 1:
            raise ValueError(f'Unsupported or already instrumented USA camera at {address:08X}')
        source = source.replace(expected, f'L_{address:08X}:\n    portablekit::fu::modern_camera::{callback}(ctx, aot_mem);\n    {statement}')
    return f'#include "{header.resolve().as_posix()}"\n' + source
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'output', 'header'): parser.add_argument(name, type=Path)
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding='utf-8'), args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != result:
        args.output.write_text(result, encoding='utf-8')
