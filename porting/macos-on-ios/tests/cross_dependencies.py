"""Classify real-format binary headers and merge isolated public SDK prefixes."""
import importlib.util,struct,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('cross',root/'arm64_dependencies.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
java=b'\xca\xfe\xba\xbe'+struct.pack('>HH',0,52)+b'\0'*128
assert not m.is_macho(java), 'Java class magic must not reach lipo'
fat=b'\xca\xfe\xba\xbe'+struct.pack('>I',2)+struct.pack('>IIIII',0x0100000c,0,4096,64,12)+struct.pack('>IIIII',0x01000007,0,8192,64,12)
assert m.is_macho(fat) and not m.is_macho(fat[:28])
assert m.is_macho(b'\xcf\xfa\xed\xfe'+b'\0'*28)
assert not m.is_macho(b'plain data')
with tempfile.TemporaryDirectory(prefix='ngb-cross-deps-') as t:
 root=Path(t);source=root/'public-keg/lib';(source/'pkgconfig').mkdir(parents=True);(source/'pkgconfig/glib.pc').write_text('fixture')
 prefix=root/'isolated';m.mirror(source,prefix/'lib');assert (prefix/'lib/pkgconfig/glib.pc').read_text()=='fixture'
 tool=root/'public-keg/bin/glslang';tool.parent.mkdir();tool.write_text('public ARM tool fixture')
 m.mirror(tool.parent,prefix/'bin');assert (prefix/'bin/glslang').is_file()
 text='@@HOMEBREW_PREFIX@@/opt/glib @@HOMEBREW_CELLAR@@/glib/1 /opt/homebrew/lib'
 result=m.relocated(text,prefix);assert '@@' not in result and '/opt/homebrew' not in result and str(prefix/'Cellar/glib/1') in result
print('PASS: Java versus fat/thin Mach-O, malformed headers, isolated prefix relocation and library merge')
