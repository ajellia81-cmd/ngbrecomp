#!/usr/bin/env python3
"""Export an explicit public-source handoff; never collect a working directory."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
FILES = (
    'docs/SMOOTH_MOTION_PORTING.md',
    'tools/export_smooth_motion.py',
    'LICENSE', 'THIRD_PARTY.md',
    'third_party/vf3_scene/README.md',
    'third_party/vf3_scene/LICENSE',
    'third_party/vf3_scene/r3d_scene.h',
    'third_party/vf3_scene/r3d_scene.cpp',
    'third_party/vf3_scene/interpolate.cpp',
    'tests/check_vf3_reference.py', 'tests/vf3_scene_test.cpp',
    'src/d3d/gpu/scene_motion.h', 'src/d3d/gpu/scene_motion.cpp',
    'src/d3d/gpu/motion_budget.h', 'src/d3d/gpu/upload_ring.h',
    'src/d3d/vk/motion_replay.inc', 'src/d3d/vk/backend.cpp',
    'xboxrecomp/src/nv2a_vk/host/pair_mailbox.h',
    'xboxrecomp/src/nv2a_vk/host/nv2a_vk_share.h',
    'xboxrecomp/src/nv2a_vk/host/present_sdl.c',
    'ios/motion_test.cpp', 'ios/presenter_pair_test.py',
    'ios/presenter_descriptor_test.py',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    tracked = set(subprocess.check_output(
        ['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0'))
    missing = set(FILES) - tracked
    if missing:
        raise SystemExit('Commit the handoff sources first: ' + ', '.join(sorted(missing)))
    if subprocess.run(['git', 'diff', '--quiet', 'HEAD', '--', *FILES], cwd=ROOT).returncode:
        raise SystemExit('Commit handoff changes before exporting an identified source revision')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    payload = {}
    for name in FILES:
        path = ROOT / name
        if path.is_symlink() or not path.resolve().is_relative_to(ROOT):
            raise SystemExit('Only regular repository files may be exported: ' + name)
        payload[name] = path.read_bytes()
    manifest = {
        'format': 1,
        'repository': 'https://github.com/ajellia81-cmd/ngbrecomp',
        'source_revision': revision,
        'entrypoint': 'docs/SMOOTH_MOTION_PORTING.md',
        'common_core_reference': 'a8473a99091e6bb207c85d6d247eece8c4866b39',
        'note': 'Common core is standalone. NGB renderer files are integration references, not a drop-in library.',
        'files': {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                  for name, data in payload.items()},
    }
    payload['MANIFEST.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(args.output, 'w:gz') as archive:
        for name, data in payload.items():
            info = tarfile.TarInfo('smooth-motion-kit/' + name)
            info.size = len(data)
            info.mode = 0o644
            archive.addfile(info, io.BytesIO(data))
    print(f'Exported {len(FILES)} public source files from {revision}: {args.output}')


if __name__ == '__main__':
    main()
