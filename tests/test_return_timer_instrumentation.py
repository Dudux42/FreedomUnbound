import importlib.util
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('return_instrument',ROOT/'scripts/instrument-return-timer.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class Instrumentation(unittest.TestCase):
    def test_preserves_original_return_and_existing_hooks(self):
        source='L_0886941C:\n    existing_hook();\n    jump_target = ctx.gpr[31];\nL_08869420:\n'
        result=module.instrument(source,ROOT/'host/return_timer.hpp')
        self.assertEqual(result.count('return_timer::refresh'),1)
        self.assertIn('existing_hook();\n    jump_target = ctx.gpr[31];',result)
        for invalid in ['',source+source,result,source.replace('jump_target','changed')]:
            with self.assertRaises(ValueError):module.instrument(invalid,ROOT/'host/return_timer.hpp')
if __name__=='__main__':unittest.main()
