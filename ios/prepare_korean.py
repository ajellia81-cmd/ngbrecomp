#!/usr/bin/env python3
"""Prepare private Korean input and a data-only on-device patch capsule.

Never run a downloaded installer. The patch ZIP is pinned; both the owner USA
input and every resulting executable section must match expected fingerprints.
Outputs contain game-derived data and MUST remain private/ignored.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path, PurePosixPath
import struct
import urllib.request
import zipfile

PATCH_URL = 'https://github.com/slimshadykor-cmd/korean-patches/releases/download/ngb-v1.2/NGB_Korean_Patch_v1.2.zip'
PATCH_SHA256 = 'c7df8ebc34b75077e75647f89606ea31eef5860d21bf3416941b0c3e5bba19d2'
USA_SHA256 = '5fa3997b0d518a9db3867ca6ae2942c99e5349fd138f536ff56772e1a0202200'
FONT_WRAPPERS = {0x001E6D30, 0x001E86A0}
MANUAL_VARIANTS = FONT_WRAPPERS | {0x0021C660}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def apply_xbe(source, plan):
    if sha(source) != USA_SHA256:
        raise ValueError('The verified owner USA executable is required')
    output = bytearray(source)
    for change in plan['xbe']['changes']:
        offset = change['offset']
        data = base64.b64decode(change['data'], validate=True)
        if offset < 0 or offset > len(output) or offset + len(data) > 64 * 1024 * 1024:
            raise ValueError('Executable patch range is invalid')
        output[offset:offset + len(data)] = data
    if 'output_length' in plan['xbe']:
        del output[plan['xbe']['output_length']:]
    if output[:4] != b'XBEH':
        raise ValueError('Invalid output executable')
    base = struct.unpack_from('<I', output, 0x104)[0]
    count, table = struct.unpack_from('<II', output, 0x11C)
    if count != 17 or table < base or table - base + count * 56 > len(output):
        raise ValueError('Unexpected output section table')
    # The release's known instruction/constant false matches must stay intact.
    for at, size in [(0x17712D, 6), (0x17769D, 6), (0x4060A8, 4)]:
        if output[at:at + size] != source[at:at + size]:
            raise ValueError('Protected instruction/constant changed')
    for index in range(count):
        entry = table - base + index * 56
        fields = struct.unpack_from('<9I20s', output, entry)
        offset, length = fields[3:5]
        if offset + length > len(output):
            raise ValueError('Output section exceeds file')
        digest = hashlib.sha1(struct.pack('<I', length) + output[offset:offset + length]).digest()
        if digest != fields[9]:
            raise ValueError(f'Patched section {index} is incompatible with USA input')
    return bytes(output)


def prepare(source_path, archive_path, output):
    archive = archive_path.read_bytes()
    if sha(archive) != PATCH_SHA256:
        raise ValueError('Korean patch ZIP fingerprint mismatch')
    output.mkdir(parents=True, exist_ok=True)
    assets = output / 'KoreanPatch'
    assets.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive_path) as package:
        plan = json.loads(package.read('ngb_native11/assets/build_plan.json'))
        candidate = apply_xbe(source_path.read_bytes(), plan)
        for archive_spec in plan['archives']:
            for member in archive_spec['members']:
                relative = PurePosixPath(archive_spec['path']) / member['name']
                if relative.is_absolute() or '..' in relative.parts or '\\' in str(relative):
                    raise ValueError('Unsafe patch member path')
                data = package.read('ngb_native11/assets/members/' + str(relative))
                if sha(data) != member['output_sha256']:
                    raise ValueError('Patch member fingerprint mismatch')
                target = assets / 'members' / str(relative)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
    capsule = {
        'patch_zip_sha256': PATCH_SHA256,
        'source_xbe_sha256': USA_SHA256,
        'output_xbe_sha256': sha(candidate),
        'output_xbe_size': len(candidate),
        'xbe': plan['xbe'], 'archives': plan['archives'],
        'root_patches': plan.get('root_patches', []),
    }
    (assets / 'manifest.json').write_text(json.dumps(capsule), encoding='utf-8')
    (output / 'korean.xbe').write_bytes(candidate)
    manual = json.loads((Path(__file__).resolve().parents[1] / 'src/manual_functions.json').read_text())
    manual = [address for address in manual if int(address, 16) not in MANUAL_VARIANTS]
    (output / 'manual-korean.json').write_text(json.dumps(manual))
    print('Korean candidate: all 17 output section hashes match the release plan')
    print('Korean candidate SHA-256:', sha(candidate))
    print('Korean candidate bytes:', len(candidate))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--zip', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--download', action='store_true')
    args = parser.parse_args()
    if args.download:
        args.zip.parent.mkdir(parents=True, exist_ok=True)
        with urllib.request.urlopen(PATCH_URL, timeout=120) as response, args.zip.open('wb') as stream:
            while chunk := response.read(1024 * 1024):
                stream.write(chunk)
    prepare(args.source, args.zip, args.output)


if __name__ == '__main__':
    main()
