from pathlib import Path
import importlib.util
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('solo',ROOT/'scripts/instrument-solo-guildhall-scaling.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class InstrumentationTests(unittest.TestCase):
    def test_supported_leaf_and_guard(self):
        path=ROOT/'overlays/ovl09A5F300_game_task_36BD79C16BF70197/generated_unit_0026.cpp'
        source=path.read_text(encoding='utf-8')
        output=module.instrument(source,ROOT/'host/solo_guildhall_scaling.hpp')
        self.assertEqual(output.count('solo_guildhall_scaling::eligible('),1)
        self.assertEqual(output.count('solo_guildhall_scaling::scaled_hp('),1)
        self.assertIn('ctx.gpr[2] = (std::bit_cast<std::uint32_t>(ctx.fpr[0]));',output)
        for invalid in (output,source.replace('L_09AC91B4:', 'L_bad:'),
                        source.replace('L_09AC9124:', 'L_09AC9124:\n    invoke_chained();')):
            with self.assertRaises(ValueError):module.instrument(invalid,Path('header.hpp'))

if __name__=='__main__':unittest.main()
