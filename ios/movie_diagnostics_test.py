"""Exercise the bounded recorder and compile-time movie probes without game data."""
from pathlib import Path
import os
import subprocess
import tempfile
from wrap_game_codegen import instrument_movie, MOVIE_PROBES

ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    folder=Path(directory)
    source=folder/'recorder.c'
    source.write_text('''#include "movie_diagnostics.h"
int main(int argc,char **argv) {
 ngb_movie_diag_open(argv[1],"test-version","test-source");
 ngb_movie_diag_glyph();
 for(unsigned i=0;i<41000;i++) ngb_movie_diag_emit("cue",i,2,3,4);
 for(unsigned i=0;i<13000;i++) ngb_movie_diag_emit("ui-rect",i,2,3,4);
 return 0;
}''')
    exe=folder/'recorder'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-D_POSIX_C_SOURCE=200809L',
        '-DNGB_IOS_MOVIE_DIAGNOSTICS=1','-I'+str(ROOT/'ios'),str(source),
        str(ROOT/'ios/movie_diagnostics.c'),'-pthread','-o',str(exe)],check=True)
    subprocess.run([str(exe),str(folder)],check=True)
    lines=(folder/'movie-subtitles.log').read_text().splitlines()
    assert len(lines)==40002 and 'test-version' in lines[0]
    assert lines[-1].endswith(',00000002,00000003,00000004,1')
    layout=(folder/'ui-layout.log').read_text().splitlines()
    assert len(layout)==12002 and 'UI layout diagnostics' in layout[0]
    assert 'ui-rect' in layout[-1] and not any(',ui-' in line for line in lines)
    # Disabled builds have no calls, data writes or recorder references.
    body='void run(void) {\n'+ '\n'.join(MOVIE_PROBES) +'\n}'
    body=body.replace('RECOMP_ENTRY(0x0019A7D0u);','/* marker */\nRECOMP_ENTRY(0x0019A7D0u);')
    fixed=instrument_movie(body)
    assert fixed.count('ngb_ios_movie_probe(')==4
    for enabled in [False,True]:
        prefix='#define RECOMP_ENTRY(x) ((void)0)\nvoid ngb_ios_movie_probe(unsigned);\n'
        path=folder/('on.c' if enabled else 'off.c');path.write_text(prefix+fixed)
        cmd=[os.environ.get('CC','cc'),'-E','-P',str(path)]
        if enabled:cmd.insert(1,'-DNGB_IOS_MOVIE_DIAGNOSTICS=1')
        output=subprocess.check_output(cmd,text=True)
        assert output.count('ngb_ios_movie_probe(')==(5 if enabled else 1)
print('Movie recorder: bounded output, provenance, glyph count and disabled-build probe removal PASS')
