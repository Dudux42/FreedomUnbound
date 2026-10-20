import importlib.util
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('camera', ROOT / 'scripts/instrument-modern-camera.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
class InstrumentationTests(unittest.TestCase):
    def test_originals_and_rejection(self):
        source = ''.join(f'L_{a:08X}:\n    {s}\n' for a, (s, c) in m.POINTS.items())
        result = m.instrument(source, Path('modern_camera.hpp'))
        for a, (statement, callback) in m.POINTS.items():
            self.assertIn(f'modern_camera::{callback}(ctx, aot_mem);\n    {statement}', result)
        for invalid in ('', source + source, result, source.replace('(128)', '(129)')):
            with self.assertRaises(ValueError): m.instrument(invalid, Path('modern_camera.hpp'))
if __name__ == '__main__': unittest.main()
