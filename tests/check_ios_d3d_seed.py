"""Check the shipped code-free seed without accessing the owner's recipes."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from merge_d3d_seed import records

asset = root / 'porting/macos-on-ios/assets/d3d-seed.bin'
data = asset.read_bytes()
manifest = json.loads(asset.with_suffix('.json').read_text())
parsed = list(records(data))
assert manifest['format'] == 'NGBD3DC1'
assert len(data) == manifest['bytes']
assert hashlib.sha256(data).hexdigest() == manifest['sha256']
assert Counter(kind.decode() for kind, _, _ in parsed) == manifest['records']
assert all(kind in (b'W', b'F', b'P', b'K') for kind, _, _ in parsed)
original = (root / 'packaging/d3d-seed.bin').read_bytes()
assert set(records(original)).issubset(set(parsed)), 'Dropped an original preparation recipe'
baseline = subprocess.check_output(['git', 'show', '5fc57d84d617aa7f360cf0a5ab707dc9334f4b95:packaging/d3d-seed.bin'], cwd=root)
assert original == baseline, 'Original desktop seed was modified'
print('iOS preparation seed: checksum/layout verified; all original recipes retained; no raw program records; desktop seed unchanged')
