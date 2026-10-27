"""Verify port-only cleanup while retaining original diagnostics and captures."""
from pathlib import Path
import importlib.util
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHILD_ENV = os.environ.copy()
for key in ('NGB_TRACE_FILE', 'NGB_TRACE_FRAMES', 'NGB_TEST_HIDDEN'):
    CHILD_ENV.pop(key, None)
spec = importlib.util.spec_from_file_location('clean_bundle', ROOT/'ios/clean_bundle.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='ngb-release-scope-') as temp:
    root = Path(temp)
    bundle = root/'NinjaGaidenBlack.app'
    keep = ['Info.plist', 'NinjaGaidenBlack', 'Assets.car', 'SettingsTab-Korean.png',
            'Frameworks/MoltenVK.framework/MoltenVK', 'Frameworks/MoltenVK.framework/Info.plist',
            'KoreanPatch/manifest.json', 'KoreanPatch/assets/font.bin', 'LICENSES/vf3-scene.txt',
            'Frameworks/MoltenVK.framework/Headers/vulkan.h',
            'Frameworks/MoltenVK.framework/Modules/module.modulemap',
            'D3DShaderSeed.bin', 'original-debug.log', 'original.dSYM/Contents/Resources/DWARF/game',
            'original.a', 'ngb-trace-1/draws.txt']
    discard = ['compile-private.log', 'configure-private.log', 'platform-private.log',
               'symbols-private.log', 'build-info.txt']
    for name in keep + discard:
        p = bundle/name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(name)
    assert module.clean_bundle(bundle) == len(discard)
    assert all((bundle/n).read_text() == n for n in keep)
    assert not any((bundle/n).exists() for n in discard)
    assert module.clean_bundle(bundle) == 0
    source = root/'stdio.c'
    source.write_text('#include "platform/ios_port_diagnostics.h"\nint main(void) {\n'
                      'NGB_IOS_PORT_DIAG("port-only marker\\n");\n'
                      'puts("original stdout"); fprintf(stderr,"original stderr\\n");\n'
                      'FILE *f=fopen("normal-save", "w"); if (!f) return 2;\n'
                      'fputs("saved",f); return fclose(f); }\n')
    exe = root/'stdio-test'
    for release in (False, True):
        flags = ['-DNGB_IOS_PORT_RELEASE=1'] if release else []
        subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', *flags,
                        '-I', str(ROOT/'xboxrecomp/src'), str(source), '-o', str(exe)], check=True)
        result = subprocess.run([str(exe)], cwd=root, capture_output=True, check=True, env=CHILD_ENV)
        assert result.stdout == b'original stdout\n'
        assert b'original stderr\n' in result.stderr
        assert (b'port-only marker' in result.stderr) != release
        assert (root/'normal-save').read_text() == 'saved'
    # Compile the actual original file logger with release flags, not a copy.
    launch = (ROOT/'src/launch.c').read_text()
    body = launch[launch.index('void ngb_launch_log_to_file('):launch.index('\nstatic void window_from_env')]
    source.write_text('#include <stdio.h>\n#include <stdlib.h>\n#include <unistd.h>\n#include <sys/stat.h>\n'
                      'int ngb_mkdir(const char *p) { return mkdir(p,0700); }\n'+body+
                      '\nint main(int n,char **v) { if(n!=2)return 1; '
                      'ngb_launch_log_to_file(v[1],"original-file.log"); '
                      'puts("original file logger"); fprintf(stderr,"original error output"); return 0; }\n')
    flags = ['-DNGB_IOS_PORT_RELEASE=1', '-DNGB_IOS_RELEASE=1']
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', *flags, str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(root)], capture_output=True, check=True, env=CHILD_ENV)
    assert 'original file logger' in (root/'original-file.log').read_text()
    assert 'original error output' in (root/'original-file.log').read_text()
    # Exercise the actual F7 recording block with a scratch home, no GPU/game.
    backend = (ROOT/'src/d3d/vk/backend.cpp').read_text()
    start = backend.index('    presents_++;', backend.index('void VulkanBackend::Present('))
    body = backend[start:backend.index('    static bool hitchlog', start)]
    cpp = root/'f7.cpp'
    cpp.write_text('#include <cstdio>\n#include <cstdlib>\n#include <string>\n#include <unistd.h>\n#include <sys/stat.h>\n'
                   'const char *home; const char *ngb_home(){return home;}\n'
                   'int ngb_mkdir(const char *p){return mkdir(p,0700);}\n'
                   'int ngb_trace_request=1, trace_left_=0; unsigned presents_=0;\n'
                   'FILE *trace_=nullptr; std::string trace_dir_;\nvoid record(){\n'+body+'\n}\n'
                   'int main(int n,char **v){if(n!=2)return 1;home=v[1];record();'
                   'if(!trace_)return 2;return fclose(trace_);}\n')
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', *flags, str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(root)], capture_output=True, check=True, env=CHILD_ENV)
    assert (root/'ngb-trace-1/draws.txt').is_file()
print('Port-only cleanup passes; original console/file logging, F7 capture and save I/O stay available.')
