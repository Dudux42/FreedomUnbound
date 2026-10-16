import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('gathering', ROOT / 'scripts/instrument-gathering.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class InstrumentationTests(unittest.TestCase):
    def test_preserves_instruction(self):
        source = 'L_08845CE4:\n    ctx.gpr[2] = (2196u << 16u);\n'
        output = module.instrument(source, Path('gathering_nodes.hpp'))
        self.assertIn('gathering_nodes::update(ctx, aot_mem);\n    ctx.gpr[2] = (2196u << 16u);', output)
        with self.assertRaises(ValueError):
            module.instrument(output, Path('gathering_nodes.hpp'))

    def test_rejects_wrong_or_duplicate_instruction(self):
        source = 'L_08845CE4:\n    ctx.gpr[2] = (2196u << 16u);\n'
        for invalid in ('', source.replace('2196', '2195'), source + source):
            with self.assertRaises(ValueError):
                module.instrument(invalid, Path('gathering_nodes.hpp'))

    def test_input_hook_preserves_original_and_rejects_mismatch(self):
        statement = 'aot_mem.aot_store32(ctx.gpr[4] + static_cast<std::uint32_t>(1156), ctx.gpr[5]);'
        source = 'L_088D61FC:\n    ' + statement
        output = module.instrument_input(source, Path('gathering_nodes.hpp'))
        self.assertIn('gathering_nodes::repeat_input(ctx, aot_mem);\n    ' + statement, output)
        for invalid in ('', output, source + source, source.replace('(1156)', '(1152)')):
            with self.assertRaises(ValueError): module.instrument_input(invalid, Path('gathering_nodes.hpp'))

if __name__ == '__main__':
    unittest.main()
