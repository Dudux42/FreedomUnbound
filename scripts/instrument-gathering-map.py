"""Observe USA map projection immediately before the local hunter arrow."""
from pathlib import Path
import argparse
POINTS={'15':(0x08843A24,0x08843A2C,False),'16':(0x088441EC,0x088441F4,True)}
def instrument(source,header,kind):
    point,ret,zoomed=POINTS[kind]
    expected=f'L_{point:08X}:\n    ctx.gpr[31] = (0x{ret:08X}u);'
    if source.count(expected)!=1 or 'gathering_map::capture' in source:raise ValueError('Unsupported or already instrumented map projection')
    return f'#include "{header.resolve().as_posix()}"\n'+source.replace(expected,f'L_{point:08X}:\n    portablekit::fu::gathering_map::capture(ctx, aot_mem, {str(zoomed).lower()});\n'+expected.split('\n',1)[1])
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for arg in ('source','output','header'):parser.add_argument(arg,type=Path)
    parser.add_argument('kind',choices=POINTS);args=parser.parse_args()
    result=instrument(args.source.read_text(encoding='utf8'),args.header,args.kind)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf8')!=result:args.output.write_text(result,encoding='utf8')
