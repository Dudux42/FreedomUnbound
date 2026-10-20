"""Observe the existing USA camera-frame boundary in a build copy."""
from pathlib import Path
import argparse
def instrument(source,header):
    expected='L_0886941C:\n'
    if source.count(expected)!=1 or 'return_timer::refresh' in source:
        raise ValueError('Unsupported or already instrumented return-timer frame hook')
    tail=source.split(expected,1)[1].split('L_08869420:',1)[0]
    if tail.count('jump_target = ctx.gpr[31];')!=1:
        raise ValueError('Unsupported camera return instruction')
    source=source.replace(expected,expected+'    portablekit::fu::return_timer::refresh(aot_mem);\n')
    return f'#include "{header.resolve().as_posix()}"\n'+source
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','output','header'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    result=instrument(args.source.read_text(encoding='utf-8'),args.header)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8')!=result:
        args.output.write_text(result,encoding='utf-8')
