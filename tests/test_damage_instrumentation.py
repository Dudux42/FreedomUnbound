import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('damage', ROOT / 'scripts/instrument-damage.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class DamageHooks(unittest.TestCase):
    def test_camera_guard(self):
        point, (statement, _) = next(iter(module.CAMERA_POINTS.items()))
        source = f'L_{point:08X}:\n    {statement}'
        result = module.instrument(source, ROOT / 'host/damage_events.hpp', camera=True)
        self.assertEqual(result.count('::capture_camera('), 1)
        self.assertEqual(result.count('monster_hp::refresh(ctx, aot_mem);'), 1)
        self.assertLess(result.index('monster_hp::refresh('), result.index(statement))
        self.assertTrue(result.endswith(statement))
        with self.assertRaises(ValueError): module.instrument(result, ROOT / 'host/damage_events.hpp', camera=True)

    def test_guards_and_order(self):
        source = '\n'.join(f'L_{p:08X}:\n    {statement}' for p, (statement, _) in module.POINTS.items())
        result = module.instrument(source, ROOT / 'host/damage_events.hpp')
        self.assertEqual(result.count('::begin_hit('), 1)
        self.assertEqual(result.count('::commit_hit('), 1)
        self.assertLess(result.index('::commit_hit('), result.index(module.POINTS[0x09C44D30][0]))
        for bad in [result, source + '\n' + source, source.replace('ctx.gpr', 'different.gpr')]:
            with self.assertRaises(ValueError): module.instrument(bad, ROOT / 'host/damage_events.hpp')

if __name__ == '__main__': unittest.main()
