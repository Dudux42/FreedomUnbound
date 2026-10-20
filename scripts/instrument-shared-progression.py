"""Add a guarded participant-credit hook to a build copy of USA main unit 28."""
from pathlib import Path
import argparse

def instrument(source, header):
    expected = 'L_088751B4:\n    { const bool branch_taken = ctx.gpr[2] == 0u;'
    if source.count(expected) != 1:
        raise ValueError('Unsupported or already instrumented USA completion gate')
    result = source.replace(expected, 'L_088751B4:\n    portablekit::fu::shared_progression::completion_gate(ctx, aot_mem);\n    { const bool branch_taken = ctx.gpr[2] == 0u;')
    return f'#include "{header.resolve().as_posix()}"\n' + result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'output', 'header'): parser.add_argument(name, type=Path)
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding='utf-8'), args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != result:
        args.output.write_text(result, encoding='utf-8')
