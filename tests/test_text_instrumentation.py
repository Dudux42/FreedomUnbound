import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("text_instrument", ROOT / "scripts/instrument-text.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
HEADER = ROOT / "host/fu_text_mod.hpp"

class Instrumentation(unittest.TestCase):
    def test_points(self):
        for kind, points in module.POINTS.items():
            source = "\n".join(f"L_{p:08X}:\n    {s}" for p, (s, _) in points.items())
            result = module.instrument(source, kind, HEADER)
            self.assertEqual(result.count("fu::text::"), len(points))
            with self.assertRaises(ValueError):
                module.instrument(result, kind, HEADER)
            with self.assertRaises(ValueError):
                module.instrument(source + "\n" + source, kind, HEADER)
            with self.assertRaises(ValueError):
                module.instrument(source.replace("ctx.gpr", "changed.gpr"), kind, HEADER)

if __name__ == "__main__":
    unittest.main()
