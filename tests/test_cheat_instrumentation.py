import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("cheat_instrument", ROOT / "scripts/instrument-cheats.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class Instrumentation(unittest.TestCase):
    def test_damage_capture_precedes_cheat_without_changing_store(self):
        statement = module.POINTS["game26"][0x09AC7AD4]
        result = module.instrument("L_09AC7AD4:\n    " + statement, "game26", ROOT / "host/cheats.hpp")
        self.assertIn("damage_events.hpp", result)
        self.assertLess(result.index("::capture("), result.index("::cheat_hook("))
        self.assertTrue(result.endswith(statement))

    def test_quest_success_dispatch(self):
        source = "L_0886E838:\n    " + module.POINTS["main26"][0x0886E838]
        result = module.instrument(source, "main26", ROOT / "host/cheats.hpp")
        self.assertIn("if (portablekit::fu::quest_complete_requested(ctx, aot_mem)) goto L_0886E8E0;", result)
        self.assertTrue(result.endswith(module.POINTS["main26"][0x0886E838]))

    def test_guards(self):
        for kind, points in module.POINTS.items():
            source = "\n".join(f"L_{p:08X}:\n    {statement}" for p, statement in points.items())
            result = module.instrument(source, kind, ROOT / "host/cheats.hpp")
            self.assertEqual(result.count("::cheat_hook("), len(points))
            with self.assertRaises(ValueError):
                module.instrument(result, kind, ROOT / "host/cheats.hpp")
            with self.assertRaises(ValueError):
                module.instrument(source + "\n" + source, kind, ROOT / "host/cheats.hpp")
            with self.assertRaises(ValueError):
                module.instrument(source.replace("ctx.gpr", "changed.gpr"), kind, ROOT / "host/cheats.hpp")

if __name__ == "__main__":
    unittest.main()
