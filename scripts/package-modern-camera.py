"""Package the configuration for the compiled-in modern-camera provider."""

import json
from pathlib import Path
import zipfile


ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.2.0"
manifest = {
    "id": "modern-camera",
    "name": "Modern Right-Stick Camera",
    "version": VERSION,
    "author": "DuduX42",
    "type": "camera",
    "api_version": 1,
    "game_id": "ULUS10391",
    "description": "Quest-only free right-stick camera, plus ZL/LT/L2 snap-to-target and ZR/RT/R2 target cycling. Marks the selected monster in its HP bar, or shows a target icon and name without the HP overlay. Requires the updated native launcher.",
    "requires": ["modern_camera", "target_camera"],
    "conflicts": [],
    "affects_save": False,
    "load_order": 0,
}


def main():
    assert manifest["id"] == "modern-camera" and manifest["type"] == "camera"
    destination = ROOT / "build" / "mods"
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / f"modern-camera-{VERSION}.zip"
    files = {
        "mod.json": (json.dumps(manifest, indent=2) + "\n").encode("utf-8"),
        "MODERN_CAMERA_MOD_GUIDE.md": (
            ROOT / "MODERN_CAMERA_MOD_GUIDE.md"
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
        assert check.read("MODERN_CAMERA_MOD_GUIDE.md") == files["MODERN_CAMERA_MOD_GUIDE.md"]
    print(archive)
    print(f"Verified {len(files)} root entries; {archive.stat().st_size} bytes")


if __name__ == "__main__":
    main()
