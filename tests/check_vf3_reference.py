"""Verify the port contains exactly the pinned VF3 interpolation implementation."""
from pathlib import Path
import hashlib
root = Path(__file__).resolve().parents[1] / 'third_party/vf3_scene'
raw = (root / 'r3d_scene.cpp').read_bytes()
assert hashlib.sha256(raw).hexdigest() == '4ff0ee6f950dbd63eb06732e723d6413e1be0f5addfa9dfd1d518ca5ec3072a7'
source = raw.decode()
start = source.index('Mat4 Mat4::identity()')
end = source.index('\nnamespace {', start)
tail = source.index('\nnamespace {\n\nfloat moved2(')
actual = (root / 'interpolate.cpp').read_text()
assert actual[actual.index('Mat4 Mat4::identity()'):] == source[start:end] + source[tail:]
print('Pinned VF3 source and unchanged interpolation section verified')
