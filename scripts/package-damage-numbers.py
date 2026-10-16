"""Package the configuration for the compiled-in damage-number provider."""

import json
from pathlib import Path
import zipfile


ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.2.1"
manifest = {
    "id": "damage-numbers",
    "name": "Damage Numbers",
    "version": VERSION,
    "author": "DuduX42",
    "type": "overlay",
    "api_version": 1,
    "game_id": "ULUS10391",
    "description": (
        "Bold damage with a thin white outline above attack impacts: light orange hits, a small * for positive "
        "criticals, and red negative-affinity hits. Requires the modified native "
        "launcher with damage-numbers support. Combat validation pending."
    ),
    "requires": ["overlay_draw", "damage_events"],
    "conflicts": [],
    "affects_save": False,
    "load_order": 0,
}


def main():
    destination = ROOT / "build" / "mods"
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / f"damage-numbers-{VERSION}.zip"
    files = {
        "mod.json": (json.dumps(manifest, indent=2) + "\n").encode("utf-8"),
        "DAMAGE_NUMBERS_MOD_GUIDE.md": (
            ROOT / "DAMAGE_NUMBERS_MOD_GUIDE.md"
        ).read_bytes(),
    }
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
        for name, data in files.items():
            entry = zipfile.ZipInfo(name, date_time=(2026, 10, 15, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            output.writestr(entry, data)
    with zipfile.ZipFile(archive) as check:
        assert check.testzip() is None
        assert set(check.namelist()) == set(files)
        assert json.loads(check.read("mod.json")) == manifest
        assert check.read("DAMAGE_NUMBERS_MOD_GUIDE.md") == files["DAMAGE_NUMBERS_MOD_GUIDE.md"]
    print(archive)
    print(f"Verified {len(files)} root entries; {archive.stat().st_size} bytes")


if __name__ == "__main__":
    main()
