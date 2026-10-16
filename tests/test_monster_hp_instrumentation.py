import importlib.util
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('hp', ROOT / 'scripts/instrument-monster-hp.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class InstrumentationTests(unittest.TestCase):
    def test_guard_and_original_instruction(self):
        source = 'L_09AC7AF4:\n    ctx.gpr[2] = (0u + static_cast<std::uint32_t>(9));\n'
        result = module.instrument(source, Path('monster_hp.hpp'))
        self.assertIn('monster_hp::capture(ctx, aot_mem);\n    ctx.gpr[2] = (0u + static_cast<std::uint32_t>(9));', result)
        for invalid in ('', source + source, source.replace('(9)', '(8)'), result):
            with self.assertRaises(ValueError):
                module.instrument(invalid, Path('monster_hp.hpp'))

    def test_reward_transition_clears_without_damage_or_camera(self):
        statement = 'aot_mem.aot_store8(ctx.gpr[3] + static_cast<std::uint32_t>(46), static_cast<std::uint8_t>(ctx.gpr[4]));'
        source = ''.join(f'L_{address:08X}:\n    {statement}\n' for address in (0x0886F0BC, 0x0886F0F8))
        result = module.lifecycle(source, Path('monster_hp.hpp'))
        self.assertEqual(result.count('monster_hp::clear();'), 2)
        self.assertEqual(result.count(statement), 2)
        for invalid in ('', source + source, source.replace('(46)', '(45)'), result):
            with self.assertRaises(ValueError):
                module.lifecycle(invalid, Path('monster_hp.hpp'))

if __name__ == '__main__':
    unittest.main()
