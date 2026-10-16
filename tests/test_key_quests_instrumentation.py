import importlib.util
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('key_instrument',ROOT/'scripts/instrument-key-quests.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
class Instrumentation(unittest.TestCase):
    def test_hooks_preserve_originals(self):
        source='\n'.join(f'L_{p:08X}:\n    ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-{n}));' for p,n in module.POINTS.items())
        result=module.instrument(source,ROOT/'host/key_quests.hpp')
        self.assertEqual(result.count('key_quests::capture'),len(module.POINTS))
        for p,n in module.POINTS.items():
            self.assertIn(f'0x{p:08X}u);\n    ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-{n}));',result)
        for invalid in ['',source+source,result,source.replace('-864','-863')]:
            with self.assertRaises(ValueError):module.instrument(invalid,ROOT/'host/key_quests.hpp')
if __name__=='__main__':unittest.main()
