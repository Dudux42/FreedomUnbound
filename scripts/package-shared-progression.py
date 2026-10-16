"""Package configuration for the compiled Shared Gathering Hall Progression provider."""
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.1.0"
manifest = {
    "id": "shared-guild-progression",
    "name": "Shared Gathering Hall Progression",
    "version": VERSION,
    "author": "DuduX42",
    "type": "qol",
    "api_version": 1,
    "game_id": "ULUS10391",
    "description": "Shares Gathering Hall urgent completion with joined participants using the game's normal clear handling. Ordinary Hall quest credit remains shared. Each participant needs this mod enabled in the updated native launcher.",
    "requires": ["shared_guild_progression"],
    "conflicts": [],
    "affects_save": True,
    "load_order": 0,
}

def main():
    destination = ROOT / "build" / "mods"
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / f"shared-guild-progression-{VERSION}.zip"
    files = {
        "mod.json": (json.dumps(manifest, indent=2) + "\n").encode(),
        "SHARED_GUILD_PROGRESSION_MOD_GUIDE.md": (ROOT / "SHARED_GUILD_PROGRESSION_MOD_GUIDE.md").read_bytes(),
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
        assert all(check.read(name) == data for name, data in files.items())
    print(archive)
    print(f"Verified {len(files)} entries; {archive.stat().st_size} bytes")

if __name__ == "__main__":
    main()
