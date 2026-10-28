#!/usr/bin/env python3
"""Merge private D3D recipes into a seed without embedding vertex programs.

Usage: merge_d3d_seed.py --xbe /scratch/default.xbe --output /scratch/seed.bin
                       packaging/d3d-seed.bin /scratch/d3d/recipes.bin

Matches CacheRecord's V-to-W conversion. No shader compilation is performed.
The output contains executable offsets, render states and pipeline descriptions;
it must not contain V records or driver/SPIR-V cache files.
"""
import argparse
from collections import Counter
from pathlib import Path
import struct

MAGIC = b'NGBD3DC1'
HEADER = struct.Struct('<cQI')
SIZES = {b'W': 19, b'F': 39, b'P': 340, b'K': 256}


def records(data):
    if not data.startswith(MAGIC):
        raise ValueError('not an NGB D3D recipe file')
    offset = len(MAGIC)
    while offset < len(data):
        # The original reader permits headers appended by earlier builds.
        if data[offset:offset + 8] == MAGIC:
            offset += 8
            continue
        if offset + HEADER.size > len(data):
            raise ValueError('incomplete recipe header; keep the original file')
        kind, key, size = HEADER.unpack_from(data, offset)
        offset += HEADER.size
        if size > 65536 or offset + size > len(data):
            raise ValueError('incomplete or oversized recipe; keep the original file')
        payload = data[offset:offset + size]
        offset += size
        if kind == b'V':
            if size < 15:
                raise ValueError('short vertex recipe')
            words, = struct.unpack_from('<I', payload, 11)
            if not words or words > 136 * 4 or size != 15 + words * 4:
                raise ValueError('unsupported vertex recipe layout')
        elif kind not in SIZES or size != SIZES[kind]:
            raise ValueError('unsupported recipe layout')
        yield kind, key, payload


def merge(inputs, xbe):
    modules = {}
    pipelines = {}
    counts = Counter()
    for data in inputs:
        for kind, key, payload in records(data):
            if kind == b'V':
                program = payload[15:]
                at = xbe.find(program)
                if at < 0:
                    counts['programs_not_in_executable'] += 1
                    continue
                words, = struct.unpack_from('<I', payload, 11)
                payload = payload[:11] + struct.pack('<II', at, words)
                kind = b'W'
                counts['programs_replaced_with_offsets'] += 1
            if kind == b'W':
                at, words = struct.unpack_from('<II', payload, 11)
                if not words or words > 136 * 4 or at + words * 4 > len(xbe):
                    raise ValueError('vertex reference is outside the supplied executable')
            if kind == b'K':
                pipelines[payload] = key
            else:
                identity = (kind == b'P', key)
                previous = modules.get(identity)
                if previous and previous != (kind, key, payload):
                    raise ValueError('conflicting module recipes; do not merge different game versions')
                modules[identity] = kind, key, payload
    output = bytearray(MAGIC)
    def emit(kind, key, payload):
        output.extend(HEADER.pack(kind, key, len(payload)))
        output.extend(payload)
        counts[kind.decode()] += 1
    for record in modules.values():
        emit(*record)
    for payload, key in pipelines.items():
        vs, ps = struct.unpack_from('<QQ', payload)
        if (False, vs) not in modules or (True, ps) not in modules:
            # Internal/specialized shaders without a recreatable recipe cannot
            # be prewarmed by the original reader. Never invent their inputs.
            counts['pipelines_without_recipes'] += 1
            continue
        emit(b'K', key, payload)
    assert all(kind != b'V' for kind, _, _ in records(output))
    return bytes(output), counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xbe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('recipes', type=Path, nargs='+')
    args = parser.parse_args()
    if args.output.resolve() in [p.resolve() for p in [args.xbe, *args.recipes]]:
        parser.error('output must not overwrite an input')
    try:
        output, counts = merge([p.read_bytes() for p in args.recipes], args.xbe.read_bytes())
        # Exclusive creation protects an existing prepared seed and all inputs.
        with args.output.open('xb') as target:
            target.write(output)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
    print(dict(counts))
    print('No vertex program bytes, SPIR-V or driver cache records emitted.')


if __name__ == '__main__':
    main()
