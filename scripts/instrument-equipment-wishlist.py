"""Observe USA smithy input and material text in build copies only."""
from pathlib import Path
import argparse
POINTS={
    'pages':{0x09A81180:16},
    'craft':{0x09AA60B4:None},
    'menu':{0x09A838E0:96,0x09A83CD8:144},
    'text':{0x08891474:864,0x088915B0:896,0x0889198C:32,0x08891A28:32},
}
MATERIALS={0x09A84030:(18,4,False),0x09A83FC4:(19,0,True)}
def instrument(source,header,kind):
    if 'wishlist::capture_' in source:raise ValueError('Already instrumented wishlist observer')
    for point,size in POINTS[kind].items():
        label=f'L_{point:08X}:\n'
        statement='ctx.gpr[2] = (ctx.gpr[17] + 0u);' if kind=='craft' else f'ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-{size}));'
        if source.count(label)!=1:raise ValueError('Unsupported wishlist observation point')
        body=source.split(label,1)[1].split('\nL_',1)[0]
        if body.count(statement)!=1:raise ValueError('Unsupported wishlist original instruction')
    for point in POINTS[kind]:
        call='capture_craft(ctx, aot_mem)' if kind=='craft' else 'capture_menu(ctx, aot_mem)' if kind in ('menu','pages') else f'capture_text(ctx, aot_mem, 0x{point:08X}u)'
        label=f'L_{point:08X}:\n'
        source=source.replace(label,label+f'    portablekit::fu::wishlist::{call};\n')
    if kind=='menu':
        for point,(register,offset,upgrade) in MATERIALS.items():
            expected=f'L_{point:08X}:\n    ctx.gpr[3] = (aot_mem.aot_load16(ctx.gpr[{register}] + static_cast<std::uint32_t>({offset})));'
            if source.count(expected)!=1:raise ValueError('Unsupported native material row')
            source=source.replace(expected,f'L_{point:08X}:\n    portablekit::fu::wishlist::capture_material(ctx, aot_mem, {str(upgrade).lower()});\n'+expected.split('\n',1)[1])
    return f'#include "{header.resolve().as_posix()}"\n'+source
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','output','header'):parser.add_argument(name,type=Path)
    parser.add_argument('kind',choices=POINTS)
    args=parser.parse_args();result=instrument(args.source.read_text(encoding='utf-8'),args.header,args.kind)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8')!=result:args.output.write_text(result,encoding='utf-8')
