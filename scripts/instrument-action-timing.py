"""Create observation-only copies of supported USA generated units.

Inputs and outputs contain game-derived code and must remain outside Git.
The originals are never edited. Missing or ambiguous labels fail the build.
"""
import argparse
from pathlib import Path


def instrument(source: str, points: tuple[int, ...], header: Path) -> str:
    for point in points:
        label = f"L_{point:08X}:\n"
        if source.count(label) != 1:
            raise ValueError(f"Expected one exact label for 0x{point:08X}")
        call = f"    portablekit::fu::sample_action_timing(0x{point:08X}u, ctx);\n"
        source = source.replace(label, label + call)
    return f'#include "{header.resolve().as_posix()}"\n' + source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("points", nargs="+", type=lambda s: int(s, 0))
    args = parser.parse_args()
    result = instrument(args.source.read_text(encoding="utf-8"), tuple(args.points), args.header)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(result, encoding="utf-8")


if __name__ == "__main__":
    main()
