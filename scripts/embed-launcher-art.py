"""Embed the launcher illustration so portable builds need no external art file."""
import pathlib
import sys

source, destination = map(pathlib.Path, sys.argv[1:3])
symbol = sys.argv[3] if len(sys.argv) > 3 else "kLauncherArt"
data = source.read_bytes()
destination.parent.mkdir(parents=True, exist_ok=True)
rows = [", ".join(f"0x{byte:02x}" for byte in data[i:i + 24]) for i in range(0, len(data), 24)]
destination.write_text(f"#pragma once\ninline constexpr unsigned char {symbol}[] = {{\n" + ",\n".join(rows) + "\n};\n", encoding="utf-8")
