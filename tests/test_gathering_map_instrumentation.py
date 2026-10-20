from pathlib import Path
import importlib.util
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('gathering_map',ROOT/'scripts/instrument-gathering-map.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class Hooks(unittest.TestCase):
    def test_observer_preserves_native_arrow(self):
        for kind,(point,ret,zoomed) in module.POINTS.items():
            source=f'L_{point:08X}:\n    ctx.gpr[31] = (0x{ret:08X}u);\n    existing_observer();\n'
            result=module.instrument(source,ROOT/'host/gathering_map.hpp',kind)
            self.assertIn(f'capture(ctx, aot_mem, {str(zoomed).lower()})',result)
            self.assertIn('existing_observer();',result)
            self.assertIn(f'ctx.gpr[31] = (0x{ret:08X}u);',result)
            for bad in ('',source+source,result,source.replace('ctx.gpr[31]','changed')):
                with self.assertRaises(ValueError):module.instrument(bad,ROOT/'host/gathering_map.hpp',kind)
if __name__=='__main__':unittest.main()
