"""Synthetic artwork tests; no copyrighted game images or disc data."""
import configparser
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

spec = importlib.util.spec_from_file_location('composition', Path(__file__).resolve().parents[1] / 'scripts/compose-texture-packs.py')
composition = importlib.util.module_from_spec(spec)
spec.loader.exec_module(composition)
KEY = '000000000000000011111111'
OTHER = '000000000000000022222222'


def image(color: bytes) -> bytes:
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b'\0' + color)) + chunk(b'IEND', b'')


class CompositionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def pack(self, name, mappings, color=b'\xff\0\0\xff', options='hash=xxh64\nignoreAddress=true', filters=''):
        folder = self.root / name
        folder.mkdir()
        (folder / 'art.png').write_bytes(image(color))
        (folder / 'textures.ini').write_text(f'[options]\n{options}\n[hashes]\n{mappings}\n{filters}')
        return folder

    def test_priority_dedup_filter_and_originals(self):
        base = self.pack('base', f'{KEY}=art.png\n{OTHER}=art.png', filters=f'[filtering]\n{KEY}=nearest')
        hud = self.pack('hud', f'{KEY}=art.png', color=b'\0\xff\0\xff')
        originals = {p: p.read_bytes() for folder in (base, hud) for p in folder.iterdir()}
        output = self.root / 'combined'
        result = composition.compose([base, hud], output)
        cfg = configparser.ConfigParser()
        cfg.read(output / 'textures.ini')
        self.assertEqual(len(result['overrides']), 1)
        self.assertEqual((output / cfg['hashes'][KEY]).read_bytes(), (hud / 'art.png').read_bytes())
        self.assertEqual((output / cfg['hashes'][OTHER]).read_bytes(), (base / 'art.png').read_bytes())
        self.assertNotIn(KEY, cfg['filtering'])  # winning layer uses auto
        self.assertTrue(all(p.read_bytes() == data for p, data in originals.items()))
        second = self.root / 'repeat'
        composition.compose([base, hud], second)
        self.assertEqual((output / 'textures.ini').read_bytes(), (second / 'textures.ini').read_bytes())
        self.assertEqual((output / 'composition.json').read_bytes(), (second / 'composition.json').read_bytes())

    def test_empty_override_uses_original_and_deduplicates(self):
        base = self.pack('base', f'{KEY}=art.png\n{OTHER}=art.png')
        hud = self.pack('hud', f'{KEY}=')
        output = self.root / 'combined'
        composition.compose([base, hud], output)
        cfg = configparser.ConfigParser()
        cfg.read(output / 'textures.ini')
        self.assertEqual(cfg['hashes'][KEY], '')
        self.assertEqual(len(list((output / 'images').iterdir())), 1)

    def test_incompatible_hash_and_bool_normalization(self):
        base = self.pack('base', f'{KEY}=art.png')
        hud = self.pack('hud', f'{OTHER}=art.png', options='hash=xxh32\nignoreAddress=true')
        with self.assertRaises(ValueError):
            composition.compose([base, hud], self.root / 'bad')
        self.assertFalse((self.root / 'bad').exists())
        (hud / 'textures.ini').write_text(f'[options]\nhash=xxh64\nignoreAddress=1\n[hashes]\n{OTHER}=art.png')
        composition.compose([base, hud], self.root / 'good')

    def test_paths_keys_and_png_rejected_without_partial_output(self):
        for index, value in enumerate(['../escape.png', 'C:/escape.png', '/escape.png', 'art.ktx2']):
            with self.subTest(value=value):
                source = self.pack(f'pack{index}', f'{KEY}={value}')
                output = self.root / f'output{index}'
                with self.assertRaises((ValueError, OSError)):
                    composition.compose([source], output)
                self.assertFalse(output.exists())
        source = self.pack('bad-key', 'INVALID=art.png')
        with self.assertRaises(ValueError):
            composition.compose([source], self.root / 'bad-key-output')
        source = self.pack('bad-png', f'{KEY}=art.png')
        (source / 'art.png').write_bytes(b'not a png')
        with self.assertRaises(ValueError):
            composition.compose([source], self.root / 'bad-png-output')

    def test_existing_and_nested_output_protected(self):
        source = self.pack('base', f'{KEY}=art.png')
        before = hashlib.sha256((source / 'textures.ini').read_bytes()).digest()
        with self.assertRaises(ValueError):
            composition.compose([source], source)
        with self.assertRaises(ValueError):
            composition.compose([source], source / 'output')
        self.assertEqual(before, hashlib.sha256((source / 'textures.ini').read_bytes()).digest())

    def test_unsupported_implicit_mapping_and_sections_rejected(self):
        source = self.pack('base', f'{KEY}=art.png')
        (source / f'{OTHER}.png').write_bytes(image(b'\0\0\xff\xff'))
        with self.assertRaises(ValueError):
            composition.compose([source], self.root / 'implicit-output')
        self.assertFalse((self.root / 'implicit-output').exists())
        source = self.pack('ranges', f'{KEY}=art.png')
        with (source / 'textures.ini').open('a') as stream:
            stream.write('\n[hashranges]\n0,128,128=64,64\n')
        with self.assertRaises(ValueError):
            composition.compose([source], self.root / 'range-output')


if __name__ == '__main__':
    unittest.main()
