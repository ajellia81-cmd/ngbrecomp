"""Check exact VF3 production source and interpolation extraction."""
from pathlib import Path
import hashlib
root=Path(__file__).resolve().parents[1]/'third_party/vf3_scene'
hashes={'r3d_scene.cpp': '8733102f5496622f3c1236af2e1a2b47734fa1a52f3b51e03b92b744fa606795', 'r3d_scene.h': 'd9d9dadd84142ff02a1ff6d00329ce09df056ba80c6030104ca9599fd0e763b7'}
for path,digest in hashes.items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
s=(root/'r3d_scene.cpp').read_text();a=s.index('Mat4 Mat4::identity()');b=s.index('\nnamespace {',a);c=s.index('\nnamespace {\n\nfloat moved2(')
e=(root/'interpolate.cpp').read_text();assert e[e.index('Mat4 Mat4::identity()'):]==s[a:b]+s[c:]
print('Latest VF3 842151aa production interpolation is unchanged, including effect exclusions')
