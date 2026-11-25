"""Verify the observer flag reaches the actual producer, not only the executable."""
from pathlib import Path
import argparse,json,os,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--commands',type=Path);args=p.parse_args()
if args.commands:
    commands=json.loads(args.commands.read_text())
    for suffix in ('src/ingame_menu.c','src/pause_tab.c','porting/macos-on-ios/bootstrap.m','porting/macos-on-ios/ui_prompt_probe.cpp'):
        rows=[r for r in commands if r['file'].endswith(suffix)]
        assert rows, suffix
        assert all('NGB_IOS_UI_PROMPT_PROBE=1' in (r.get('command') or ' '.join(r['arguments'])) for r in rows),suffix
    print('PASS: actual producer, bootstrap and collector compile commands enable the probe')
else:
    s=(root/'porting/macos-on-ios/target.cmake').read_text()
    block=s[s.index('option(NGB_IOS_UI_PROMPT_PROBE'):]
    for enabled in (True,False):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder)
            (d/'producer.c').write_text(('#ifndef' if enabled else '#ifdef')+' NGB_IOS_UI_PROMPT_PROBE\n#error wrong producer flag\n#endif\nint ngb_ui_prompt_hook_enabled(void){return 1;}\n')
            (d/'main.cpp').write_text(('#ifndef' if enabled else '#ifdef')+' NGB_IOS_UI_PROMPT_PROBE\n#error wrong executable flag\n#endif\nextern "C" int ngb_ui_prompt_hook_enabled(void);\nint main(){return ngb_ui_prompt_hook_enabled()!=1;}\n')
            (d/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(probe LANGUAGES C CXX)\nset(CMAKE_CXX_STANDARD 17)\nadd_library(ngb_gen OBJECT producer.c)\nadd_executable(probe main.cpp $<TARGET_OBJECTS:ngb_gen>)\nfind_package(Threads REQUIRED)\ntarget_link_libraries(probe PRIVATE Threads::Threads)\nset(NGB_CONVERSION_DIR "'+str(root/'porting/macos-on-ios')+'")\n'+block)
            cmake=os.environ.get('CMAKE','cmake')
            subprocess.run([cmake,'-S',str(d),'-B',str(d/'build'),'-DNGB_IOS_UI_PROMPT_PROBE='+('ON' if enabled else 'OFF')],check=True,stdout=subprocess.DEVNULL)
            subprocess.run([cmake,'--build',str(d/'build')],check=True,stdout=subprocess.DEVNULL)
            subprocess.run([str(d/'build/probe')],check=True)
    print('PASS: real target block builds both observer producer and executable, probe ON and OFF')
