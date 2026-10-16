from pathlib import Path
import importlib.util
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('shared', ROOT/'scripts/instrument-shared-progression.py')
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
class SharedInstrumentationTests(unittest.TestCase):
    def test_scoped_original_branch(self):
        source='L_088751B4:\n    { const bool branch_taken = ctx.gpr[2] == 0u;\n'
        output=module.instrument(source,Path('shared_progression.hpp'))
        self.assertIn('completion_gate(ctx, aot_mem);\n    { const bool branch_taken = ctx.gpr[2] == 0u;',output)
        for invalid in ('',output,source+source,source.replace('== 0u','!= 0u')):
            with self.assertRaises(ValueError):module.instrument(invalid,Path('shared_progression.hpp'))
if __name__=='__main__':unittest.main()
