"""Compose compatible flat PNG packs, lowest priority first. Source packs stay unchanged."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path, PureWindowsPath
import re
import shutil
import struct
import tempfile
import zlib

KEY = re.compile(r'[0-9a-fA-F]{24}\Z')
DEFAULTS = {'version': '1', 'ignoreaddress': 'false', 'reducehash': 'false',
            'ignoremipmap': 'false', 'skiplastdxt1blocks128x64': '0', 'skiplastdxt1blocks128x128': '0'}


def png(path: Path) -> str:
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'Not a PNG: {path.name}')
    pos, first, ended, image_data = 8, True, False, False
    while pos + 12 <= len(data):
        size = struct.unpack_from('>I', data, pos)[0]
        kind = data[pos + 4:pos + 8]
        end = pos + 12 + size
        if end > len(data):
            raise ValueError(f'Truncated PNG: {path.name}')
        payload = data[pos + 8:pos + 8 + size]
        crc = struct.unpack_from('>I', data, pos + 8 + size)[0]
        if zlib.crc32(kind + payload) != crc:
            raise ValueError(f'PNG checksum error: {path.name}')
        if first:
            if kind != b'IHDR' or size != 13:
                raise ValueError(f'Invalid PNG header: {path.name}')
            width, height = struct.unpack_from('>II', payload)
            if not (0 < width <= 16384 and 0 < height <= 16384):
                raise ValueError(f'Invalid PNG dimensions: {path.name}')
            first = False
        pos = end
        image_data |= kind == b'IDAT' and size > 0
        if kind == b'IEND':
            ended = size == 0 and pos == len(data)
            break
    if not ended or not image_data:
        raise ValueError(f'Invalid PNG end: {path.name}')
    return hashlib.sha256(data).hexdigest()


def read_pack(folder: Path) -> tuple[dict, dict, dict]:
    cfg = configparser.ConfigParser(interpolation=None, strict=True, delimiters=('=',))
    cfg.optionxform = str
    with (folder / 'textures.ini').open(encoding='utf-8-sig') as stream:
        cfg.read_file(stream)
    if cfg.defaults() or set(cfg.sections()) - {'options', 'hashes', 'filtering'}:
        raise ValueError('Use flat packs with [options], [hashes], and optional [filtering]; game overrides/ranges need flattening first.')
    if not cfg.has_section('options') or not cfg.has_section('hashes'):
        raise ValueError('Pack needs [options] and [hashes].')
    options = DEFAULTS | {k.lower(): v.strip().lower() for k, v in cfg.items('options')}
    if set(options) - (set(DEFAULTS) | {'hash'}) or options.get('hash') not in {'xxh32', 'xxh64'} or options['version'] != '1':
        raise ValueError('Unsupported pack options/hash/version.')
    for key in ('ignoreaddress', 'reducehash', 'ignoremipmap'):
        if options[key] not in {'true', 'false', '1', '0'}:
            raise ValueError(f'Invalid boolean option: {key}')
        options[key] = 'true' if options[key] in {'true', '1'} else 'false'
    for key in ('skiplastdxt1blocks128x64', 'skiplastdxt1blocks128x128'):
        if not options[key].isdigit() or int(options[key]) > 0xFFFFFFFF:
            raise ValueError(f'Invalid integer option: {key}')
        options[key] = str(int(options[key]))
    entries = {}
    for key, value in cfg.items('hashes'):
        if not KEY.fullmatch(key) or key.lower() in entries:
            raise ValueError(f'Invalid or duplicate texture key: {key}')
        value = value.strip().replace('\\', '/')
        if not value:
            entries[key.lower()] = None
            continue
        relative = Path(value)
        if relative.is_absolute() or PureWindowsPath(value).drive or '..' in relative.parts:
            raise ValueError('Image path escapes source pack.')
        image = (folder / relative).resolve()
        if not image.is_relative_to(folder.resolve()) or image.suffix.lower() != '.png':
            raise ValueError('Image must be a PNG inside its source pack.')
        entries[key.lower()] = (image, png(image))
    if not entries:
        raise ValueError('Empty texture mapping.')
    for image in folder.iterdir():
        if image.is_file() and image.suffix.lower() == '.png' and KEY.fullmatch(image.stem[:24]):
            if (len(image.stem) == 24 or image.stem[24:25] == '_') and image.stem[:24].lower() not in entries:
                raise ValueError('Implicit hash-named images need explicit [hashes] mappings before composition.')
    filters = {}
    if cfg.has_section('filtering'):
        for key, value in cfg.items('filtering'):
            if not KEY.fullmatch(key) or key.lower() not in entries or value.lower() not in {'auto', 'nearest', 'linear'}:
                raise ValueError('Invalid or unmapped filtering entry.')
            filters[key.lower()] = value.lower()
    return options, entries, filters


def compose(layers: list[Path], output: Path) -> dict:
    if not layers:
        raise ValueError('At least one layer is required.')
    output = output.resolve()
    layers = [p.resolve() for p in layers]
    if output.exists():
        raise ValueError('Output already exists; choose a new directory.')
    if any(output.is_relative_to(p) or p.is_relative_to(output) for p in layers):
        raise ValueError('Output must be separate from source packs.')
    selected, options, overridden = {}, None, []
    for index, folder in enumerate(layers):
        current, entries, filters = read_pack(folder)
        if options is not None and current != options:
            raise ValueError('Hashing options differ between packs; they cannot be safely combined.')
        options = current
        for key, asset in entries.items():
            if key in selected:
                overridden.append({'key': key, 'from_layer': selected[key][0], 'to_layer': index})
            selected[key] = (index, asset, filters.get(key, 'auto'))
    output.parent.mkdir(parents=True, exist_ok=True)
    # Build in a sibling staging directory; validation errors leave no partial pack.
    with tempfile.TemporaryDirectory(prefix='.pack-compose-', dir=output.parent) as staging_name:
        staging = Path(staging_name)
        (staging / 'images').mkdir()
        cfg = configparser.ConfigParser(interpolation=None)
        cfg['options'] = options
        cfg['hashes'], cfg['filtering'] = {}, {}
        provenance = []
        for key, (layer, asset, filtering) in sorted(selected.items()):
            destination = ''
            if asset is not None:
                image, content_hash = asset
                destination = f'images/{content_hash}.png'
                target = staging / destination
                if not target.exists():
                    shutil.copyfile(image, target)
                if png(target) != content_hash:
                    raise ValueError('Source image changed during composition.')
            cfg['hashes'][key] = destination
            if filtering != 'auto':
                cfg['filtering'][key] = filtering
            provenance.append({'key': key, 'layer': layer, 'image': destination})
        with (staging / 'textures.ini').open('w', encoding='utf-8') as stream:
            cfg.write(stream)
        record = {'version': 1, 'layer_count': len(layers), 'texture_keys': len(selected),
                  'priority': 'later layers override earlier layers', 'overrides': overridden,
                  'textures': provenance}
        (staging / 'composition.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
        # Rename only into a still-absent destination; never replace a user's pack.
        if output.exists():
            raise ValueError('Output appeared during composition.')
        staging.rename(output)
    return record


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--layer', action='append', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        record = compose(args.layer, args.output)
    except (ValueError, OSError, configparser.Error) as error:
        parser.exit(1, f'Composition failed: {error}\n')
    print(json.dumps({'texture_keys': record['texture_keys'], 'overrides': len(record['overrides'])}))


if __name__ == '__main__':
    main()
