"""Independent synthetic save fixtures; never reads a player's save."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

PROBE = sys.argv[1]
ROOT = Path(__file__).resolve().parents[1]
# The game's save table is game data and is not in the repo. Point MHFU_SAVE_TABLE
# at a local 256-byte copy to run this test; without one it is skipped.
TABLE_PATH = os.environ.get("MHFU_SAVE_TABLE")
if not TABLE_PATH or not Path(TABLE_PATH).is_file():
    print("skipped: set MHFU_SAVE_TABLE to a local copy of the save table")
    sys.exit(0)
TABLE = Path(TABLE_PATH).read_bytes()
assert len(TABLE) == len(set(TABLE)) == 256
INVERSE = bytes(TABLE.index(v) for v in range(256))


def encoded(slots=(), seed=0x12345678, bad_marker=False, bad_rank=False):
    payload = bytearray(0x16a0e8)
    for i, (name, sex, rank, money, seconds) in enumerate(slots):
        base = i * 0x6b100
        name_bytes = name.encode("utf-16le")
        assert len(name_bytes) <= 16
        payload[base + 0x1000:base + 0x1000 + len(name_bytes)] = name_bytes
        payload[base + 0x1012] = sex
        payload[base + 0x501c] = rank - 1
        struct.pack_into("<I", payload, base + 0x4fc8, seconds)
        struct.pack_into("<I", payload, base + 0x6a250, money)
        struct.pack_into("<II", payload, base + 0x6a248, 123+i, 9876+i)
        for gear in range(6):
            struct.pack_into("<BBH", payload, base + 0x1020+gear*12, 1, 5 if gear==0 else gear-1, 30+i+gear)
    if bad_marker:
        payload[0x1012] = 3
    if bad_rank:
        payload[0x501c] = 9
    checksum = hashlib.sha1(payload[:0x16a0dc] + b"3Nc94Hq1zOLh8d62Sb69").digest()
    stage = (payload + checksum).translate(INVERSE)
    words = list(struct.unpack(f"<{len(stage)//4}I", stage))
    high, low = seed >> 16 or 0xdfa3, seed & 65535 or 0x215f
    for i in range(len(words)):
        high = high * 0xdfa3 % 0xffef
        low = low * 0x215f % 0xff8f
        words[i] ^= high * 65536 + low
    body = struct.pack(f"<{len(words)}I", *words).translate(INVERSE)
    return body + struct.pack("<I", seed).translate(INVERSE).translate(INVERSE)


def run(mode, path, *args):
    return subprocess.check_output([PROBE, mode, str(path), *map(str, args)], timeout=15).decode("utf-8").splitlines()


def hashes(folder):
    return {str(p.relative_to(folder)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in folder.rglob("*") if p.is_file()}


with tempfile.TemporaryDirectory(prefix="fu-save-preview-") as temp:
    work = Path(temp)
    blob = work / "synthetic.bin"
    assert run("folder", work / "missing")[0] == "missing"
    blob.write_bytes(encoded())
    assert run("inner", blob) == ["ready"] + ["0||0|0|0|0:00:00"] * 3
    hunters = [("First", 1, 1, 0, 0), ("Éowyn", 2, 5, 12345, 3661), ("𐐀Eight", 1, 9, 9999999, 36000059)]
    expected = ["ready", "1|First|1|0|0|0:00:00", "1|Éowyn|5|12345|3661|1:01:01", "1|𐐀Eight|9|9999999|36000059|10000:00:59"]
    good = encoded(hunters)
    blob.write_bytes(good)
    assert run("inner", blob) == expected
    blob.write_bytes(encoded(hunters, seed=0))
    assert run("inner", blob) == expected
    for broken in [good[:-1], good + b"\0", good[:100], bytes(len(good)),
                   good[:123] + bytes([good[123] ^ 1]) + good[124:],
                   encoded(hunters, bad_marker=True), encoded(hunters, bad_rank=True)]:
        blob.write_bytes(broken)
        assert run("inner", blob)[0] == "unavailable"
    blob.write_bytes(good)
    memory = work / "ms0"
    assert run("folder", memory)[0] == "missing"
    run("record", blob, memory)
    saved = memory / "PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN"
    summary = work / "save-preview/ULUS10391.txt"
    assert summary.is_file()
    before = hashes(memory)
    assert run("folder", memory) == expected
    details=run("inner-details",blob)
    assert run("folder-details",memory)==details
    assert details[1]=="1|124|9877|1:5:31|1:1:33|1:2:34|1:3:35|1:4:36|1:0:32"
    # Old summaries do not invent point balances or equipment.
    original_summary=summary.read_text(encoding="utf-8")
    summary.write_text("\n".join(line for line in original_summary.splitlines()
                                if not line.startswith(("details ","gear ")))+"\n",encoding="utf-8")
    assert run("folder",memory)==expected
    assert run("folder-details",memory)==["0|0|0"+"|0:0:0"*6]*3
    # Invalid optional rows cannot index beyond the three hunters or six items.
    summary.write_text(original_summary+"gear 0 99 1 5 3\ngear 9 0 1 5 3\ngear 0 0 1 99 3\ndetails 99 100 100\n",encoding="utf-8")
    assert run("folder-details",memory)==details
    assert hashes(memory) == before, "Preview must never modify save files"
    # A save changed outside the game waits until the game sees it again.
    stamp = saved.stat().st_mtime_ns + 10**9
    os.utime(saved, ns=(stamp, stamp))
    assert run("folder", memory)[0] == "pending"
    run("record", blob, memory)
    assert run("folder", memory) == expected
    summary.write_text("FUPREVIEW 1\nstamp 0\n", encoding="utf-8")
    assert run("folder", memory)[0] == "pending"
    run("record", blob, memory)
    lines = summary.read_text(encoding="utf-8").splitlines()
    lines[2] = "slot 0 1 99 0 0 Bad"
    summary.write_text("\n".join(lines) + "\n", encoding="utf-8")
    assert run("folder", memory)[0] == "unavailable"
    summary.unlink()
    assert run("folder", memory)[0] == "pending"
    broken = work / "broken.bin"
    broken.write_bytes(good[:100])
    run("record", broken, memory)
    assert not summary.exists(), "A save that fails its checksum leaves no summary"
    print("Save preview: three slots, rank boundaries, Unicode, time, seed defaults, integrity, key-free summary, and read-only checks passed.")
