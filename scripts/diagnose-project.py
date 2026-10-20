"""Read-only local build audit. Does not launch binaries or inspect save contents."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def imports(path: Path) -> list[str]:
    data = path.read_bytes()
    if data[:2] != b'MZ':
        raise ValueError('Not a Windows executable')
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE header')
    sections, opt_size = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from('<H', data, optional)[0]
    directories = optional + {0x10B: 96, 0x20B: 112}[magic]
    table = optional + opt_size

    def offset(rva: int) -> int:
        for n in range(sections):
            virtual_size, virtual, raw_size, raw = struct.unpack_from('<4I', data, table + n * 40 + 8)
            if virtual <= rva < virtual + max(virtual_size, raw_size):
                result = raw + rva - virtual
                if rva - virtual >= raw_size or result >= len(data):
                    raise ValueError('Import points outside file-backed section')
                return result
        headers = struct.unpack_from('<I', data, optional + 60)[0]
        if rva < min(headers, len(data)):
            return rva
        raise ValueError('Unmapped import RVA')

    def name(rva: int) -> str:
        start = offset(rva)
        end = data.index(b'\0', start, min(len(data), start + 512))
        return data[start:end].decode('ascii')

    result = set()
    for directory, stride in [(1, 20), (13, 32)]:
        rva, size = struct.unpack_from('<II', data, directories + directory * 8)
        if not rva:
            continue
        start = offset(rva)
        for pos in range(start, start + size, stride):
            descriptor = struct.unpack_from('<' + 'I' * (stride // 4), data, pos)
            if not any(descriptor):
                break
            if directory == 13 and not descriptor[0] & 1:
                raise ValueError('Unsupported VA-based delay imports')
            result.add(name(descriptor[1] if directory == 13 else descriptor[3]))
    return sorted(result, key=str.lower)


def audit(root: Path, build: Path, packages: list[Path]) -> dict:
    binary = build / 'bin'
    search = [binary, Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32']
    search += [Path(p) for p in os.environ.get('PATH', '').split(os.pathsep) if p]
    inventory = {}
    for folder in search:
        if folder.is_dir():
            for item in folder.iterdir():
                if item.is_file():
                    inventory.setdefault(item.name.lower(), item)
    binaries = [binary / 'FreedomUniteNative.exe'] + sorted(binary.rglob('*.dll'))
    if (binary / 'FreedomUnbound.exe').is_file():
        binaries.append(binary / 'FreedomUnbound.exe')
    missing, errors, names, contracts = [], [], set(), set()
    for path in binaries:
        try:
            for dependency in imports(path):
                names.add(dependency)
                if dependency.lower().startswith(('api-ms-', 'ext-ms-')):
                    contracts.add(dependency)
                elif dependency.lower() not in inventory:
                    missing.append({'binary': str(path.relative_to(binary)), 'dependency': dependency})
        except (OSError, ValueError, KeyError, struct.error) as error:
            errors.append({'binary': str(path.relative_to(binary)), 'error': str(error)})
    profile = (root / 'host/freedom_unite_profile.cpp').read_text()
    expected = re.search(r'\.executable_sha256\s*=\s*"([0-9a-f]{64})"', profile).group(1)
    elf = build / 'game/EBOOT.ELF'
    record_path = build / 'build-info.json'
    record = json.loads(record_path.read_text(encoding='utf-8-sig')) if record_path.exists() else {}
    stale = [item['file'] for item in record.get('source_files', [])
             if not (root / item['file']).is_file() or digest(root / item['file']).lower() != item['sha256'].lower()]
    package_results = []
    for package in packages:
        manifest = json.loads((package / 'build-files.json').read_text(encoding='utf-8-sig'))
        invalid = []
        for item in manifest:
            path = package / item['file']
            if not path.is_file() or path.stat().st_size != item['bytes'] or digest(path).lower() != item['sha256'].lower():
                invalid.append(item['file'])
        package_results.append({'package': package.name, 'files': len(manifest), 'invalid': invalid})
    tracked = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode().split('\0')
    forbidden = [p for p in tracked if p and (p.startswith(('generated/', 'overlays/', 'build/', 'baselines/'))
                 or Path(p).suffix.lower() in {'.iso', '.elf', '.exe', '.dll', '.sav', '.ovl'})]
    return {'pe_binaries_checked': len(binaries), 'overlay_libraries': len(list((binary / 'overlays').glob('*.dll'))),
            'imported_library_names': sorted(names), 'api_set_contracts': sorted(contracts),
            'missing_dependencies': missing, 'pe_parse_errors': errors,
            'supported_executable_hash_matches': elf.is_file() and digest(elf) == expected,
            'build_executable_hash_matches': bool(record) and digest(binary / 'FreedomUniteNative.exe').lower() == record['executable_sha256'].lower(),
            'build_launcher_hash_matches': 'launcher_sha256' not in record or (
                (binary / 'FreedomUnbound.exe').is_file() and
                digest(binary / 'FreedomUnbound.exe').lower() == record['launcher_sha256'].lower()),
            'stale_source_files': stale, 'action_timing_diagnostics': record.get('action_timing_diagnostics'),
            'packages': package_results, 'forbidden_tracked_files': forbidden,
            'limits': ['File-presence import audit, not export-symbol or driver compatibility verification.',
                       'API-set contracts are delegated to Windows; runtime-loaded plugins are not enumerated.',
                       'No gameplay, save-content, multiplayer, or native-60-FPS validation.']}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build/windows'))
    parser.add_argument('--package', action='append', type=Path, default=[])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    result = audit(root, root / args.build, args.package)
    encoded = json.dumps(result, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding='utf-8')
    print(encoded)
    failed = result['missing_dependencies'] or result['pe_parse_errors'] or result['stale_source_files'] or result['forbidden_tracked_files']
    failed = failed or not result['supported_executable_hash_matches'] or not result['build_executable_hash_matches']
    failed = failed or not result['build_launcher_hash_matches']
    failed = failed or any(p['invalid'] for p in result['packages'])
    raise SystemExit(1 if failed else 0)


if __name__ == '__main__':
    main()
