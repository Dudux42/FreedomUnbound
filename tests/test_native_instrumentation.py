"""Check research hooks reject unfamiliar inputs and preserve unrelated code."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("native_instrument", ROOT / "scripts/instrument-native-60.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
HEADER = ROOT / "host/native_60.hpp"


class InstrumentTests(unittest.TestCase):
    def test_blink_hooks_leave_shared_clock_untouched(self):
        for unit, point, register in (("0013", 0x0883B728, 3), ("0015", 0x08841B58, 2), ("0060", 0x088F5FFC, 3)):
            source = f"prefix\nL_{point:08X}:\n    ctx.gpr[{register}] = (aot_mem.aot_load32(ctx.gpr[{register}] + static_cast<std::uint32_t>(-23212)));\nshared increment preserved\n"
            output = module.instrument(source, unit, HEADER)
            self.assertEqual(output.count("native_ui_clock("), 1)
            self.assertTrue(output.endswith("shared increment preserved\n"))
            for bad in (source.replace("(-23212)", "(-23208)"), source + source):
                with self.assertRaises(ValueError): module.instrument(bad, unit, HEADER)

    def test_fade_observation_precedes_compensation(self):
        instructions = ((0x0884D348, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(1));"),
                        (0x0884D418, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(1));"),
                        (0x0884D754, "aot_mem.aot_store16(ctx.gpr[4] + static_cast<std::uint32_t>(36), static_cast<std::uint16_t>(ctx.gpr[5]));"))
        for observed in (False, True):
            source = "".join(f"L_{point:08X}:\n" + (f"    portablekit::fu::sample_action_timing(0x{point:08X}u, ctx);\n" if observed else "") + f"    {instruction}\n" for point, instruction in instructions)
            output = module.instrument(source, "0018", HEADER)
            self.assertEqual(output.count("native_fade_tick("), 2)
            self.assertEqual(output.count("native_fade_reset("), 1)
            if observed: self.assertLess(output.index("sample_action_timing"), output.index("native_fade_tick"))
            for bad in (source.replace("L_0884D418", "L_0884D41C"), source + source):
                with self.assertRaises(ValueError): module.instrument(bad, "0018", HEADER)

    def test_repeat_hook_preserves_fresh_events_and_thresholds(self):
        source = "fresh events preserved\nL_08869398:\n    ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(1));\nthreshold 13 and reset 10 preserved\nL_088693D4:\n    aot_mem.aot_store8(ctx.gpr[16] + static_cast<std::uint32_t>(20), static_cast<std::uint8_t>(0u));\nrelease handling preserved\n"
        output = module.instrument(source, "0025", HEADER)
        self.assertEqual(output.count("native_repeat_tick("), 1)
        self.assertEqual(output.count("native_repeat_reset("), 1)
        self.assertIn("threshold 13 and reset 10 preserved\n", output)
        self.assertTrue(output.endswith("release handling preserved\n"))
        for bad in (source.replace("L_08869398", "L_08869394"), source.replace("(20)", "(21)"), source + source):
            with self.assertRaises(ValueError): module.instrument(bad, "0025", HEADER)

    def test_gate_preserves_surrounding_code(self):
        instruction = "ctx.gpr[3] = (static_cast<std::int32_t>(ctx.gpr[4]) < 2 ? 1u : 0u);"
        message_instructions = (
            (0x0885A120, "aot_mem.aot_store16(ctx.gpr[16] + static_cast<std::uint32_t>(24), static_cast<std::uint16_t>(ctx.gpr[3]));"),
            (0x0885A838, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(-1));"),
            (0x0885A7E0, "ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1];"),
            (0x0885A8A8, "ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2];"),
            (0x0885A8E4, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(1));"),
            (0x0885A8F0, "ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-16));"))
        fixture = "".join(f"L_{point:08X}:\n    {value}\n" for point, value in message_instructions)
        source = fixture + f"prefix\nL_088591A4:\n    {instruction}\n    goto next;\nsuffix\n"
        output = module.instrument(source, "0021", HEADER)
        self.assertIn("prefix\nL_088591A4:", output)
        self.assertTrue(output.endswith("    goto next;\nsuffix\n"))
        self.assertIn("native_frame_threshold()", output)
        for hook, count in (("native_message_reset", 1), ("native_message_tick", 3),
                            ("native_message_motion", 2), ("native_message_finish", 1)):
            self.assertEqual(output.count(hook + "("), count)
        # Deferred cleanup returns to the original caller before touching the
        # stack, active flag, owned text, or tracker lifetime.
        cleanup = output.split("L_0885A8F0:\n", 1)[1]
        self.assertLess(cleanup.index("ctx.pc = ctx.gpr[31]; return;"), cleanup.index("native_message_finish"))
        self.assertLess(cleanup.index("native_message_finish"), cleanup.index("ctx.gpr[29] ="))
        # Every message instruction is mandatory; a shifted site must fail
        # closed rather than silently enabling only the frame gate.
        for point, _ in message_instructions:
            with self.assertRaises(ValueError):
                module.instrument(source.replace(f"L_{point:08X}:", "missing:"), "0021", HEADER)
        for bad in ("", source + source, source.replace("< 2", "< 3")):
            with self.assertRaises(ValueError):
                module.instrument(bad, "0021", HEADER)

    def test_countdown_composes_with_observer(self):
        for observer in ("", "    portablekit::fu::sample_action_timing(0x0886DFACu, ctx);\n"):
            original = "L_0886DFAC:\n" + observer + "    ctx.gpr[3] = (2205u << 16u);\n    goto L_0886DFB0;\n"
            output = module.instrument(original, "0026", HEADER)
            self.assertEqual(output.count("native_countdown_step(ctx)"), 1)
            if observer:
                self.assertLess(output.index("sample_action_timing"), output.index("native_countdown_step"))
            with self.assertRaises(ValueError):
                module.instrument(original.replace("2205u", "2204u"), "0026", HEADER)

    def test_animation_consumers_include_inlined_delay(self):
        def load(register):
            return f"ctx.fpr[{register}] = std::bit_cast<float>(aot_mem.aot_load32(ctx.gpr[17] + static_cast<std::uint32_t>(20)));"
        original = "preserved rate producer\n" + "".join(
            f"L_{point:08X}:\n    {load(register)}\n"
            for point, register in ((0x088638C0, 1), (0x088638D4, 0),
                                    (0x08863978, 1), (0x08863994, 1),
                                    (0x08863BE8, 0), (0x08863C44, 0), (0x08863CFC, 0)))
        original += f"L_08863974:\n    if (ctx.gpr[20] != 0u) {{\n    {load(1)}\n        goto target;\n    }}\n"
        original += "L_08863894:\n    portablekit::fu::sample_action_timing(0x08863894u, ctx);\n    ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2];\nL_088638A0:\n    ctx.gpr[2] = (ctx.gpr[2] + static_cast<std::uint32_t>(-1));\nL_088638BC:\n    aot_mem.aot_store8(ctx.gpr[17] + static_cast<std::uint32_t>(62), static_cast<std::uint8_t>(0u));\n"
        output = module.instrument(original, "0023", HEADER)
        self.assertEqual(output.count("native_animation_step("), 8)
        self.assertEqual(output.count("native_blend_increment("), 1)
        self.assertEqual(output.count("native_blend_tick("), 1)
        self.assertEqual(output.count("native_blend_finish("), 1)
        self.assertLess(output.index("sample_action_timing(0x08863894"), output.index("native_blend_increment(ctx.fpr[2]"))
        self.assertIn("preserved rate producer\n", output)
        self.assertIn("goto target;", output)
        for bad in (original.replace("L_08863C44", "L_08863C40"),
                    original.replace("L_08863CFC", "L_08863CF8"),
                    original.replace("ctx.gpr[20] != 0u", "ctx.gpr[20] == 0u"),
                    original + original):
            with self.assertRaises(ValueError):
                module.instrument(bad, "0023", HEADER)

    def test_blend_setup_keeps_initial_pose_and_releases_cancelled_channel(self):
        source = "L_0886401C:\n    portablekit::fu::sample_action_timing(0x0886401Cu, ctx);\n    aot_mem.aot_store32(ctx.gpr[5] + static_cast<std::uint32_t>(48), ctx.gpr[17]);\ninitial weight and increment preserved\nL_088640B0:\n    aot_mem.aot_store8(ctx.gpr[4] + static_cast<std::uint32_t>(62), static_cast<std::uint8_t>(0u));\n"
        source += "L_08864368:\n    ctx.fpr[0] = std::bit_cast<float>(aot_mem.aot_load32(ctx.gpr[3] + static_cast<std::uint32_t>(20)));\n"
        output = module.instrument(source, "0024", HEADER)
        self.assertEqual(output.count("native_blend_reset("), 1)
        self.assertEqual(output.count("native_blend_finish("), 1)
        self.assertEqual(output.count("native_animation_step("), 1)
        self.assertIn("initial weight and increment preserved\n", output)
        self.assertLess(output.index("sample_action_timing"), output.index("native_blend_reset"))
        for bad in (source.replace("(48)", "(44)"), source.replace("L_088640B0", "L_088640AC"), source.replace("L_08864368", "L_08864364"), source + source):
            with self.assertRaises(ValueError): module.instrument(bad, "0024", HEADER)

    def test_event_windows_do_not_overlap_at_native_cadence(self):
        # Same source timeline at twice the update cadence. An event must fire
        # once, including fractional boundaries, rather than once per half-step.
        for rate in (0.5, 1.0, 2.0, 2.5):
            for target in (0.0, 0.25, 1.0, 3.75, 7.0):
                native_step = rate * 0.5
                hits = sum(frame * native_step <= target < (frame + 1) * native_step
                           for frame in range(64))
                self.assertEqual(hits, 1)
        # The previously unscaled query inspects overlapping intervals.
        self.assertEqual(sum(frame <= 3.75 < frame + 2.0 for frame in range(8)), 2)


if __name__ == "__main__":
    unittest.main()
