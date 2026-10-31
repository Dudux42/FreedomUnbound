"""Scale the HP result of the supported USA monster-stat calculator only."""
from pathlib import Path
import argparse

def instrument(source, header):
    # This leaf has no calls/yields between entry and return. The local guard is
    # invocation-local, never guest register state or a cross-thread latch.
    start = source.index('L_09AC9120:')
    end = source.index('L_09AC91BC:', start)
    leaf = source[start:end]
    if 'invoke_chained' in leaf or leaf.count('jump_target = ctx.gpr[31]') != 1:
        raise ValueError('Unsupported non-leaf monster stat calculator')
    replacements = {
        '    std::uint32_t local_transfers = 0u;':
            '    bool solo_hall_hp = false;\n    std::uint32_t local_transfers = 0u;',
        'L_09AC9120:\n    ctx.gpr[6] = (aot_mem.aot_load8(ctx.gpr[4] + static_cast<std::uint32_t>(488)));':
            'L_09AC9120:\n    solo_hall_hp = portablekit::fu::solo_guildhall_scaling::eligible(ctx, aot_mem);\n    ctx.gpr[6] = (aot_mem.aot_load8(ctx.gpr[4] + static_cast<std::uint32_t>(488)));',
        'L_09AC91B4:\n    jump_target = ctx.gpr[31];':
            'L_09AC91B4:\n    if (solo_hall_hp) ctx.fpr[0] = std::bit_cast<float>(portablekit::fu::solo_guildhall_scaling::scaled_hp(std::bit_cast<std::uint32_t>(ctx.fpr[0])));\n    solo_hall_hp = false;\n    jump_target = ctx.gpr[31];',
    }
    if 'bool solo_hall_hp' in source:
        raise ValueError('Already instrumented')
    for old, new in replacements.items():
        if source.count(old) != 1:
            raise ValueError('Unsupported USA stat calculator')
        source = source.replace(old, new)
    return f'#include "{header.resolve().as_posix()}"\n'+source

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for field in ('source','output','header'): parser.add_argument(field,type=Path)
    args=parser.parse_args()
    result=instrument(args.source.read_text(encoding='utf-8'),args.header)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8')!=result:
        args.output.write_text(result,encoding='utf-8')
