"""Package configuration for the native Flag Key Quests provider."""
import json
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
VERSION='0.1.0'
manifest={
    'id':'key-quests','name':'Flag Key Quests','version':VERSION,
    'author':'DuduX42','type':'overlay','api_version':1,'game_id':'ULUS10391',
    'description':'Adds a small gold star next to key quest names in the Village and Gathering Hall quest selection pages. Uses native progression prerequisite lists and preserves quest names and save progression. Requires the updated native launcher.',
    'requires':['key_quests'],'conflicts':[],'affects_save':False,'load_order':0,
}
def main():
    destination=ROOT/'build/mods'
    destination.mkdir(parents=True,exist_ok=True)
    archive=destination/f'key-quests-{VERSION}.zip'
    files={'mod.json':(json.dumps(manifest,indent=2)+'\n').encode(),
           'KEY_QUESTS_MOD_GUIDE.md':(ROOT/'KEY_QUESTS_MOD_GUIDE.md').read_bytes()}
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as output:
        for name,data in files.items():
            entry=zipfile.ZipInfo(name,date_time=(2026,10,15,0,0,0))
            entry.compress_type=zipfile.ZIP_DEFLATED
            entry.external_attr=0o100644<<16
            output.writestr(entry,data)
    with zipfile.ZipFile(archive) as check:
        assert check.testzip() is None and set(check.namelist())==set(files)
        assert all(check.read(name)==data for name,data in files.items())
        assert json.loads(check.read('mod.json'))==manifest
    print(archive)
    print(f'Verified {len(files)} entries; {archive.stat().st_size} bytes')
if __name__=='__main__':main()
