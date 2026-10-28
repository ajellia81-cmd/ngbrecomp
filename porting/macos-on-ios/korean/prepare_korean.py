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
from korean_sprite_layout import correct_sprite_layout

PATCH_URL = 'https://github.com/slimshadykor-cmd/korean-patches/releases/download/ngb-v1.2/NGB_Korean_Patch_v1.2.zip'
PATCH_SHA256 = 'c7df8ebc34b75077e75647f89606ea31eef5860d21bf3416941b0c3e5bba19d2'
USA_SHA256 = '5fa3997b0d518a9db3867ca6ae2942c99e5349fd138f536ff56772e1a0202200'
FONT_WRAPPERS = {0x001E6D30, 0x001E86A0}
MANUAL_VARIANTS = FONT_WRAPPERS | {0x0021C660}
# Verified static call/jump targets of this pinned patch, not device observations.
CONTINUATIONS = (0x001E86A5, 0x0021C800, 0x0030E540, 0x0030E543,
                 0x00DCA200, 0x00DCA400, 0x00DCA500, 0x00DCA840)


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


def settings_tab_mask(sprite):
    # Reuse the pinned translation's own 설정 glyphs from Options, rather
    # than introducing a different system typeface. The tab art is italic.
    header=0x72C
    if sprite[:4]!=b'XPR0': raise ValueError('Invalid Korean Options sprite')
    data_start=struct.unpack_from('<I',sprite,8)[0]
    common,data,_,fmt,_=struct.unpack_from('<5I',sprite,header)
    palette=struct.unpack_from('<I',sprite,header+24)[0]
    if common!=0x40001 or (fmt>>8)&255!=11 or (fmt>>20)&15!=10 or (fmt>>24)&15!=5:
        raise ValueError('Pinned Korean settings lettering layout changed')
    def swizzled(x,y):
        out=0;bit=1;shift=0
        while bit<1024:
            out|=bool(x&bit)<<shift;shift+=1
            if bit<32: out|=bool(y&bit)<<shift;shift+=1
            bit<<=1
        return out
    source=[]
    for y in range(32):
        row=[]
        for x in range(85,124):
            index=sprite[data_start+data+swizzled(x,y)]
            color=struct.unpack_from('<I',sprite,data_start+palette+index*4)[0]
            light=sum((color>>s)&255 for s in (0,8,16))/3
            row.append(round((color>>24)*max(0,min(1,(light-70)/185))))
        source.append(row)
    occupied=[(x,y) for y,row in enumerate(source) for x,v in enumerate(row) if v>12]
    if not occupied: raise ValueError('Korean settings lettering is empty')
    y0=min(y for x,y in occupied);y1=max(y for x,y in occupied)
    scale=27/(y1-y0+1);height=27;width=round(39*scale+height*.18)+2
    mask=bytearray(width*height)
    for y in range(height):
        sy=min(y1,y0+int((y+.5)/scale));slant=(height-1-y)*.18
        for x in range(width):
            sx=int((x-slant)/scale)
            if x>=slant and 0<=sx<39: mask[y*width+x]=source[sy][sx]
    return b'NGBM'+struct.pack('<HH',width,height)+mask


def center_menu_headings(sprite):
    # These three sprites are centered headings, not shared background art.
    # The translation centers ink in the texture, but the original definitions
    # crop Main Menu and Missions to narrower, left-anchored UV intervals.
    # Move the sampling window, preserving its extent and all texture pixels.
    if sprite[:4] != b'XPR0' or struct.unpack_from('<I', sprite, 16)[0] != 120:
        raise ValueError('Pinned Korean main-menu sprite layout changed')
    output = bytearray(sprite)
    for index, width, old_end, ink_center in [(0, 256, 230, 128),
                                              (1, 512, 438, 219),
                                              (2, 256, 202, 129.5)]:
        at = 20 + index * 24
        u0, v0, u1, v1, texture, size = struct.unpack_from('<4f2I', sprite, at)
        if (u0, v0, u1, v1, texture) != (0, 0, old_end / width, 1, index):
            raise ValueError('Pinned Korean heading bounds changed')
        shift = (ink_center - old_end / 2) / width
        struct.pack_into('<f', output, at, u0 + shift)
        struct.pack_into('<f', output, at + 8, u1 + shift)
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
        settings_mask = None
        for archive_spec in plan['archives']:
            for member in archive_spec['members']:
                relative = PurePosixPath(archive_spec['path']) / member['name']
                if relative.is_absolute() or '..' in relative.parts or '\\' in str(relative):
                    raise ValueError('Unsafe patch member path')
                data = package.read('ngb_native11/assets/members/' + str(relative))
                if sha(data) != member['output_sha256']:
                    raise ValueError('Patch member fingerprint mismatch')
                if str(relative) == 'spr.afs/option.spr':
                    # The approved tab lettering samples the unshifted source.
                    settings_mask = settings_tab_mask(data)
                if str(relative) == 'demo.afs/main_menu.spr':
                    data = center_menu_headings(data)
                data = correct_sprite_layout(str(relative), data)
                member['output_sha256'] = sha(data)
                target = assets / 'members' / str(relative)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
    if settings_mask is None:
        raise ValueError('Korean Options lettering is missing')
    (assets/'settings-tab.mask').write_bytes(settings_mask)
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
    manual = json.loads((Path(__file__).resolve().parents[3] / 'src/manual_functions.json').read_text())
    manual = [address for address in manual if int(address, 16) not in MANUAL_VARIANTS]
    (output / 'manual-korean.json').write_text(json.dumps(manual))
    (output / 'continuations.json').write_text(json.dumps([
        {'start': f'0x{address:08X}', 'static_branch': True,
         'note': 'Static Korean patch continuation; not runtime-observed'}
        for address in CONTINUATIONS]))
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
