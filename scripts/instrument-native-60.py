"""Insert guarded research hooks into local USA generated copies, never originals.

Addresses and instruction facts were measured from the supported executable.
No community patch implementation is installed or translated here.
"""
import argparse
from pathlib import Path


def instrument(source: str, unit: str, header: Path) -> str:
    def replace(point: int, before: str, after: str) -> None:
        nonlocal source
        expected = f"L_{point:08X}:\n    {before}\n"
        observed = f"L_{point:08X}:\n    portablekit::fu::sample_action_timing(0x{point:08X}u, ctx);\n    {before}\n"
        if source.count(expected) + source.count(observed) != 1:
            raise ValueError(f"Unsupported compiled instruction at 0x{point:08X}")
        if observed in source:
            source = source.replace(observed, observed.replace(f"    {before}\n", f"    {after}\n"))
        else:
            source = source.replace(expected, f"L_{point:08X}:\n    {after}\n")

    if unit in ("0013", "0015", "0060"):
        point, register = {"0013": (0x0883B728, 3), "0015": (0x08841B58, 2),
                           "0060": (0x088F5FFC, 3)}[unit]
        # Scale only the blink consumer; the shared counter also schedules work.
        value = f"aot_mem.aot_load32(ctx.gpr[{register}] + static_cast<std::uint32_t>(-23212))"
        replace(point, f"ctx.gpr[{register}] = ({value});",
                f"ctx.gpr[{register}] = portablekit::fu::native_ui_clock({value});")
    elif unit == "0018":
        for point in (0x0884D348, 0x0884D418):
            replace(point, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(1));",
                    "ctx.gpr[3] = ctx.gpr[3] + portablekit::fu::native_fade_tick(ctx.gpr[17]);")
        replace(0x0884D754,
                "aot_mem.aot_store16(ctx.gpr[4] + static_cast<std::uint32_t>(36), static_cast<std::uint16_t>(ctx.gpr[5]));",
                "portablekit::fu::native_fade_reset(ctx.gpr[4]);\n    aot_mem.aot_store16(ctx.gpr[4] + static_cast<std::uint32_t>(36), static_cast<std::uint16_t>(ctx.gpr[5]));")
    elif unit == "0021":
        replace(0x088591A4,
                "ctx.gpr[3] = (static_cast<std::int32_t>(ctx.gpr[4]) < 2 ? 1u : 0u);",
                "ctx.gpr[3] = (static_cast<std::int32_t>(ctx.gpr[4]) < static_cast<std::int32_t>(portablekit::fu::native_frame_threshold()) ? 1u : 0u);")
        replace(0x0885A120,
                "aot_mem.aot_store16(ctx.gpr[16] + static_cast<std::uint32_t>(24), static_cast<std::uint16_t>(ctx.gpr[3]));",
                "portablekit::fu::native_message_reset(ctx.gpr[16]);\n    aot_mem.aot_store16(ctx.gpr[16] + static_cast<std::uint32_t>(24), static_cast<std::uint16_t>(ctx.gpr[3]));")
        replace(0x0885A838, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(-1));",
                "ctx.gpr[3] = ctx.gpr[3] - portablekit::fu::native_message_tick(ctx.gpr[17]);")
        replace(0x0885A7E0, "ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1];",
                "ctx.fpr[0] = ctx.fpr[0] + portablekit::fu::native_message_motion(ctx.fpr[1]);")
        replace(0x0885A8A8, "ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2];",
                "ctx.fpr[1] = ctx.fpr[1] + portablekit::fu::native_message_motion(ctx.fpr[2]);")
        replace(0x0885A8E4, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(1));",
                "ctx.gpr[3] = ctx.gpr[3] + portablekit::fu::native_message_tick(ctx.gpr[4]);")
        replace(0x0885A8F0, "ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-16));",
                "if (portablekit::fu::native_message_tick(ctx.gpr[4]) == 0u) { ctx.pc = ctx.gpr[31]; return; }\n    portablekit::fu::native_message_finish(ctx.gpr[4]);\n    ctx.gpr[29] = (ctx.gpr[29] + static_cast<std::uint32_t>(-16));")
    elif unit == "0024":
        # The event query must inspect the same next-frame interval as the
        # channel updater. Keeping the original rate overlaps adjacent native
        # intervals and can fire a threshold before the visible pose reaches it.
        replace(0x08864368,
                "ctx.fpr[0] = std::bit_cast<float>(aot_mem.aot_load32(ctx.gpr[3] + static_cast<std::uint32_t>(20)));",
                "ctx.fpr[0] = portablekit::fu::native_animation_step(std::bit_cast<float>(aot_mem.aot_load32(ctx.gpr[3] + static_cast<std::uint32_t>(20))));")
        replace(0x0886401C,
                "aot_mem.aot_store32(ctx.gpr[5] + static_cast<std::uint32_t>(48), ctx.gpr[17]);",
                "portablekit::fu::native_blend_reset(ctx.gpr[5]);\n    aot_mem.aot_store32(ctx.gpr[5] + static_cast<std::uint32_t>(48), ctx.gpr[17]);")
        replace(0x088640B0,
                "aot_mem.aot_store8(ctx.gpr[4] + static_cast<std::uint32_t>(62), static_cast<std::uint8_t>(0u));",
                "portablekit::fu::native_blend_finish(ctx.gpr[4]);\n    aot_mem.aot_store8(ctx.gpr[4] + static_cast<std::uint32_t>(62), static_cast<std::uint8_t>(0u));")
    elif unit == "0025":
        replace(0x08869398, "ctx.gpr[3] = (ctx.gpr[3] + static_cast<std::uint32_t>(1));",
                "ctx.gpr[3] = ctx.gpr[3] + portablekit::fu::native_repeat_tick(ctx.gpr[16]);")
        replace(0x088693D4,
                "aot_mem.aot_store8(ctx.gpr[16] + static_cast<std::uint32_t>(20), static_cast<std::uint8_t>(0u));",
                "portablekit::fu::native_repeat_reset(ctx.gpr[16]);\n    aot_mem.aot_store8(ctx.gpr[16] + static_cast<std::uint32_t>(20), static_cast<std::uint8_t>(0u));")
    elif unit == "0026":
        label = "L_0886DFAC:\n"
        if source.count(label) != 1:
            raise ValueError("Unsupported countdown entry")
        # Diagnostics may already insert a sample immediately after this label.
        # Keep that sample before the compensation, so it observes the caller.
        entry = source.split(label, 1)[1].split("goto L_0886DFB0;", 1)[0]
        instruction = "    ctx.gpr[3] = (2205u << 16u);\n"
        if entry.count(instruction) != 1:
            raise ValueError("Unsupported countdown instruction")
        changed = entry.replace(instruction, "    portablekit::fu::native_countdown_step(ctx);\n" + instruction)
        source = source.replace(label + entry, label + changed, 1)
    elif unit == "0023":
        replace(0x08863894, "ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2];",
                "ctx.fpr[1] = ctx.fpr[1] + portablekit::fu::native_blend_increment(ctx.fpr[2], ctx.gpr[17]);")
        replace(0x088638A0, "ctx.gpr[2] = (ctx.gpr[2] + static_cast<std::uint32_t>(-1));",
                "ctx.gpr[2] = ctx.gpr[2] - portablekit::fu::native_blend_tick(ctx.gpr[17]);")
        replace(0x088638BC,
                "aot_mem.aot_store8(ctx.gpr[17] + static_cast<std::uint32_t>(62), static_cast<std::uint8_t>(0u));",
                "portablekit::fu::native_blend_finish(ctx.gpr[17]);\n    aot_mem.aot_store8(ctx.gpr[17] + static_cast<std::uint32_t>(62), static_cast<std::uint8_t>(0u));")
        # Shared advancement and the matching root-motion lookahead queries.
        # Keep producer rates/caps intact, including movement's retained 2.5.
        def load(register: int) -> str:
            return f"ctx.fpr[{register}] = std::bit_cast<float>(aot_mem.aot_load32(ctx.gpr[17] + static_cast<std::uint32_t>(20)));"

        def scaled(register: int) -> str:
            return f"ctx.fpr[{register}] = portablekit::fu::native_animation_step(std::bit_cast<float>(aot_mem.aot_load32(ctx.gpr[17] + static_cast<std::uint32_t>(20))));"

        for point, register in ((0x088638C0, 1), (0x088638D4, 0),
                                (0x08863978, 1), (0x08863994, 1),
                                (0x08863BE8, 0), (0x08863C44, 0), (0x08863CFC, 0)):
            replace(point, load(register), scaled(register))
        # 0x08863978 is also inlined in the preceding branch's delay slot.
        delay = "L_08863974:\n    if (ctx.gpr[20] != 0u) {\n    "
        expected = delay + load(1) + "\n"
        if source.count(expected) != 1:
            raise ValueError("Unsupported animation branch delay slot")
        source = source.replace(expected, delay + scaled(1) + "\n")
    else:
        raise ValueError(f"Unsupported unit: {unit}")
    return f'#include "{header.resolve().as_posix()}"\n' + source


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("unit", choices=("0013", "0015", "0018", "0021", "0023", "0024", "0025", "0026", "0060"))
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding="utf-8"), args.unit, args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(result, encoding="utf-8")
