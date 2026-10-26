#!/usr/bin/env python3
"""Remove only known diagnostic files introduced by this iOS port."""
from pathlib import Path
import argparse
import shutil

NAMES = {'build-info.txt', 'compile-private.log', 'configure-private.log',
         'platform-private.log', 'symbols-private.log'}


def unwanted(path):
    return path.name in NAMES


def clean_bundle(bundle):
    if bundle.suffix != '.app' or not (bundle/'Info.plist').is_file() or not (bundle/'NinjaGaidenBlack').is_file():
        raise ValueError('Expected a staged real-game .app bundle')
    removed = 0
    for path in sorted(bundle.rglob('*'), key=lambda p: len(p.parts)):
        if not path.exists() and not path.is_symlink():
            continue
        if unwanted(path):
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            else:
                path.unlink()
            removed += 1
    if any(unwanted(p) for p in bundle.rglob('*')):
        raise ValueError('iOS-port diagnostic material remains in the staged bundle')
    return removed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    args = parser.parse_args()
    print('Release bundle iOS-port diagnostic entries removed:', clean_bundle(args.bundle))
