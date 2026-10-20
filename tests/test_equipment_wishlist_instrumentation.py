from pathlib import Path
import importlib.util
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('wishlist_instrument',ROOT/'scripts/instrument-equipment-wishlist.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class Instrumentation(unittest.TestCase):
    def test_validates_and_preserves_existing_observers(self):
        for kind,points in module.POINTS.items():
            source=''
            for point,size in points.items():
                statement='ctx.gpr[2] = (ctx.gpr[17] + 0u);' if kind=='craft' else f'ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-{size}));'
                source+=f'L_{point:08X}:\n    existing_observer(ctx, aot_mem);\n    {statement}\n'
            if kind=='menu':
                for point,(register,offset,_) in module.MATERIALS.items():
                    source+=f'L_{point:08X}:\n    ctx.gpr[3] = (aot_mem.aot_load16(ctx.gpr[{register}] + static_cast<std::uint32_t>({offset})));\n'
            result=module.instrument(source,ROOT/'host/equipment_wishlist.hpp',kind)
            self.assertEqual(result.count('wishlist::capture_'),len(points)+(len(module.MATERIALS) if kind=='menu' else 0))
            self.assertEqual(result.count('existing_observer'),len(points))
            for invalid in ('',source+source,result,source.replace('ctx.gpr[2] =' if kind=='craft' else 'ctx.gpr[29] =','changed =')):
                with self.assertRaises(ValueError):module.instrument(invalid,ROOT/'host/equipment_wishlist.hpp',kind)
if __name__=='__main__':unittest.main()
