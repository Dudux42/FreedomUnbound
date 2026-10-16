"""Package configuration selecting the compiled native monster HP provider."""
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = {
    'id': 'monster-hp', 'name': 'Monster HP', 'version': '0.2.1',
    'author': 'DuduX42', 'type': 'overlay', 'api_version': 1,
    'game_id': 'ULUS10391',
    'description': 'Persistent bottom-left HP bars with names for large monsters in your current area, stacked upwards, plus a 25% low-HP indicator. Requires the modified native launcher. Experimental: hunt coverage needs verification.',
    'requires': ['overlay_draw', 'monster_hp'], 'conflicts': [],
    'affects_save': False, 'load_order': 0,
}

def main():
    folder = ROOT / 'build/mods'
    folder.mkdir(parents=True, exist_ok=True)
    destination = folder / 'monster-hp-0.2.1.zip'
    files = {'mod.json': (json.dumps(MANIFEST, indent=2) + '\n').encode('utf-8'),
             'MONSTER_HP_MOD_GUIDE.md': (ROOT / 'MONSTER_HP_MOD_GUIDE.md').read_bytes()}
    with zipfile.ZipFile(destination, 'w') as archive:
        for name, contents in files.items():
            info = zipfile.ZipInfo(name, date_time=(2026, 10, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, contents)
    with zipfile.ZipFile(destination) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(files)
        for name, contents in files.items():
            assert archive.read(name) == contents
    print(destination)
    print(f'Verified {len(files)} root entries; {destination.stat().st_size} bytes')

if __name__ == '__main__':
    main()
