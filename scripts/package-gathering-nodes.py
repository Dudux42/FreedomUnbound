"""Package data selecting the native gathering-node provider."""
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.2.3'
MANIFEST = {
    'id': 'gathering-nodes', 'name': 'Gathering Nodes', 'version': VERSION,
    'author': 'DuduX42', 'type': 'overlay', 'api_version': 1,
    'game_id': 'ULUS10391',
    'description': 'A green question-mark icon above the hunter for usable gathering nodes. Hidden with a drawn weapon or depleted node; hold Circle to repeat gathering through native input. Requires the modified native launcher.',
    'requires': ['overlay_draw', 'gathering_nodes', 'gathering_repeat'], 'conflicts': [],
    'affects_save': False, 'load_order': 0,
}

def main():
    destination = ROOT / 'build/mods'
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / f'gathering-nodes-{VERSION}.zip'
    files = {
        'mod.json': (json.dumps(MANIFEST, indent=2) + '\n').encode('utf-8'),
        'GATHERING_NODES_MOD_GUIDE.md': (ROOT / 'GATHERING_NODES_MOD_GUIDE.md').read_bytes(),
    }
    with zipfile.ZipFile(archive, 'w') as output:
        for name, contents in files.items():
            entry = zipfile.ZipInfo(name, date_time=(2026, 10, 15, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            output.writestr(entry, contents)
    with zipfile.ZipFile(archive) as check:
        assert check.testzip() is None
        assert set(check.namelist()) == set(files)
        for name, contents in files.items():
            assert check.read(name) == contents
    print(archive)
    print(f'Verified {len(files)} root entries; {archive.stat().st_size} bytes')

if __name__ == '__main__':
    main()
