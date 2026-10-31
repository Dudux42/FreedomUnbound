"""camera_aim instrumentation inserts exactly one guarded call at each supported join."""
import importlib.util, pathlib, sys, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('aim', root / 'scripts' / 'instrument-camera-aim.py')
aim = importlib.util.module_from_spec(spec); spec.loader.exec_module(aim)
header = root / 'host' / 'camera_aim.hpp'
for site, (address, statement) in aim.POINTS.items():
    src = f'L_{address:08X}:\n{statement}\n'
    out = aim.instrument(src, header, site)
    assert out.count(f'camera_aim::{site}(ctx, aot_mem);') == 1, site
    assert out.index(f'camera_aim::{site}') < out.index(statement), site
    for bad in (src.replace(statement, '    ctx.gpr[5] = 0u;'), out):
        try: aim.instrument(bad, header, site)
        except ValueError: pass
        else: raise AssertionError(f'{site} accepted unsupported source')
for addr in (0x09AAA330, 0x09AAA6BC):
    assert all(a != addr for a, _ in aim.POINTS.values()), 'mounted sites must stay vanilla'
print('camera_aim instrumentation ok')
