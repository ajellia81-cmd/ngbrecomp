"""Exercise staged-bundle cleanup and quiet stdio without UIKit/game data."""
from pathlib import Path
import importlib.util
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('clean_bundle', ROOT/'ios/clean_bundle.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='ngb-release-package-') as temp:
    root = Path(temp)
    bundle = root/'NinjaGaidenBlack.app'
    keep = ['Info.plist', 'NinjaGaidenBlack', 'Assets.car', 'SettingsTab-Korean.png',
            'Frameworks/MoltenVK.framework/MoltenVK', 'Frameworks/MoltenVK.framework/Info.plist',
            'KoreanPatch/manifest.json', 'KoreanPatch/assets/font.bin', 'LICENSES/vf3-scene.txt']
    discard = ['compile-private.log', 'build-info.txt', 'sample.dSYM/Contents/Resources/DWARF/game',
               'Frameworks/MoltenVK.framework/Headers/vulkan.h',
               'Frameworks/MoltenVK.framework/Modules/module.modulemap', 'development.a', '.DS_Store']
    for name in keep + discard:
        p = bundle/name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(name)
    assert module.clean_bundle(bundle) > 0
    assert all((bundle/n).read_text() == n for n in keep)
    assert not any((bundle/n).exists() for n in discard)
    assert module.clean_bundle(bundle) == 0
    # The real release helper silences console streams without preventing
    # ordinary saves/settings from writing their own file streams.
    source = root/'stdio.c'
    source.write_text('#include "ios/release_stdio.h"\nint main(void) {\n'
                      'if (!ngb_ios_quiet_stdio()) return 1;\n'
                      'puts("no console log"); fprintf(stderr,"no error log");\n'
                      'FILE *f=fopen("normal-save", "w"); if (!f) return 2;\n'
                      'fputs("saved",f); return fclose(f); }\n')
    exe = root/'stdio-test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-I', str(ROOT), str(source), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe)], cwd=root, capture_output=True, check=True)
    assert result.stdout == result.stderr == b''
    assert (root/'normal-save').read_text() == 'saved'
print('Release cleanup preserves game assets/licenses; quiet console preserves save I/O.')
