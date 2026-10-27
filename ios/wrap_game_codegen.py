#!/usr/bin/env python3
"""Add private host boundaries without publishing generated game bodies."""
from pathlib import Path
import argparse
import re
import tempfile

WRAPPERS=('001E6AE0','0017E8A0','00232D40')
CACHE_WAIT='loc_0031C750: ;\n    PUSH32(esp, 0x32);'
PRODUCER_WAIT='    PUSH32(esp, 0x00333085u); RECOMP_ICALL_SAFE(0x003C7CB0u, _icall_esp); /* manual call 0x003C7CB0 */'
SHADER_READY={
    'loc_002E20E1: ;':'loc_002E20EE',
    'loc_002E2131: ;':'loc_002E213E',
    'loc_002E2180: ;':'loc_002E218D',
}

DEMO_READY='loc_00165DEF: ;'

def adapt_shader_ready(text):
    for marker,destination in SHADER_READY.items():
        text=text.replace(marker,marker+'\n    if (!ngb_ios_shader_loading_ready()) goto '+destination+';')
    # Only the title-idle timer's completion is held. Input handling and
    # the existing per-frame title rendering continue on their original paths.
    text=text.replace(DEMO_READY,DEMO_READY+'\n    if (!ngb_ios_shader_prewarm_ready()) goto loc_00165DF9;')
    return text

# Observe the cache consumer and its library's two ReadFile branches. These
# probes never write guest state and disappear from ordinary release builds.
CACHE_PROBES={
    'loc_0031C771: ;':'ngb_movie_diag_cache_queue(eax,esi);',
    'loc_0031C7ED: ;':'ngb_movie_diag_cache_io(1,0,MEM32(0x9B1C88),0);',
    'loc_0031C806: ;':'ngb_movie_diag_cache_io(1,1,0,eax);',
    'loc_0031C8C7: ;':'ngb_movie_diag_cache_io(1,0,esi,0);',
    'loc_0031C8DA: ;':'ngb_movie_diag_cache_io(1,1,0,eax);',
    'loc_00337583: ;':'ngb_movie_diag_cache_io(0,0,MEM32(esi+0x50),0);',
    'loc_003375AE: ;':'ngb_movie_diag_cache_io(0,1,0,eax);',
    'loc_0033765E: ;':'ngb_movie_diag_cache_io(0,0,MEM32(esi+0x50),0);',
    'loc_0033766B: ;':'ngb_movie_diag_cache_io(0,1,0,eax);',
}

def instrument_cache(text):
    for marker,call in CACHE_PROBES.items():
        text=text.replace(marker,marker+'\n#ifdef NGB_IOS_MOVIE_DIAGNOSTICS\n'
                          '    '+call+'\n#endif')
    return text

def adapt_cache_wait(text):
    text=text.replace(PRODUCER_WAIT,
        '    if (ngb_ios_cache_producer_fast()) {\n'
        '        PUSH32(esp, 1);\n'
        '        PUSH32(esp, 0x00333085u); RECOMP_ABI_CALL(0x0032B077u, sub_0032B077);\n'
        '    } else {\n'+PRODUCER_WAIT+'\n    }')
    return text.replace(CACHE_WAIT,'loc_0031C750: ;\n'
        '    PUSH32(esp, ngb_ios_cache_copy_delay(0x32u,MEM32(0xC0608C)));')

MOVIE_PROBES={
    'RECOMP_ENTRY(0x0019A7D0u);':1,
    'loc_0019A9CD: ;':2,
    'RECOMP_ENTRY(0x00232D40u);':3,
    'loc_0031E1C5: ;':4,
}

def instrument_movie(text):
    for marker,stage in MOVIE_PROBES.items():
        text=text.replace(marker,marker+'\n#ifdef NGB_IOS_MOVIE_DIAGNOSTICS\n'
                          f'    ngb_ios_movie_probe({stage});\n#endif')
    return text

def wrap(folder):
    found={address:0 for address in WRAPPERS};reads=0
    probes={marker:0 for marker in MOVIE_PROBES}
    cache_waits=0;producer_waits=0;demo_ready=0
    cache_probes={marker:0 for marker in CACHE_PROBES}
    shader_ready={marker:0 for marker in SHADER_READY}
    for path in folder.glob('recomp_*.c'):
        text=path.read_text()
        for address in WRAPPERS:
            text,n=re.subn(r'\bvoid sub_'+address+r'\(void\)\s*\{',
                'void ngb_base_'+address+'(void) {',text)
            found[address]+=n
        lines=[]
        for line in text.splitlines(keepends=True):
            if 'ngb_ios_game_text_language' not in line:
                line,n=re.subn(r'MEM(?:8|32)\(0xDB9890\)(?!\s*=(?!=))',
                    lambda m:'ngb_ios_game_text_language('+m[0]+')',line)
                reads+=n
            lines.append(line)
        result=''.join(lines)
        for marker in probes: probes[marker]+=result.count(marker)
        cache_waits+=result.count(CACHE_WAIT)
        producer_waits+=result.count(PRODUCER_WAIT)
        for marker in cache_probes: cache_probes[marker]+=result.count(marker)
        for marker in shader_ready: shader_ready[marker]+=result.count(marker)
        demo_ready+=result.count(DEMO_READY)
        path.write_text(adapt_shader_ready(adapt_cache_wait(instrument_cache(instrument_movie(result)))))
    for address,count in found.items():
        if count!=1:raise ValueError('Expected one original private wrapper body: '+address)
    if any(count!=1 for count in probes.values()):
        raise ValueError('Expected one original movie diagnostic boundary per stage')
    if cache_waits!=1:raise ValueError('Expected one original cache-worker packet wait')
    if producer_waits!=1:raise ValueError('Expected one original cache-producer display wait')
    if any(count!=1 for count in cache_probes.values()):
        raise ValueError('Expected one original cache I/O diagnostic boundary per stage')
    if any(count!=1 for count in shader_ready.values()):
        raise ValueError('Expected all original loading-ready transitions exactly once')
    if demo_ready!=1:raise ValueError('Expected one original title-idle completion')
    header=folder/'recomp_funcs.h'
    with header.open('a') as stream:stream.write('\nunsigned ngb_ios_game_text_language(unsigned saved);\n')
    with header.open('a') as stream:stream.write('\nunsigned ngb_ios_cache_copy_delay(unsigned milliseconds,unsigned state);\n')
    with header.open('a') as stream:stream.write('\nunsigned ngb_ios_cache_producer_fast(void);\n')
    with header.open('a') as stream:stream.write('\nunsigned ngb_ios_shader_loading_ready(void);\nunsigned ngb_ios_shader_prewarm_ready(void);\n')
    with header.open('a') as stream:stream.write('\n#ifdef NGB_IOS_MOVIE_DIAGNOSTICS\nvoid ngb_ios_movie_probe(unsigned stage);\n#endif\n')
    with header.open('a') as stream:stream.write('\n#ifdef NGB_IOS_MOVIE_DIAGNOSTICS\n'
        'void ngb_movie_diag_cache_queue(unsigned available,unsigned remaining);\n'
        'void ngb_movie_diag_cache_io(unsigned kind,unsigned end,unsigned bytes,unsigned result);\n#endif\n')
    if reads<10:raise ValueError('Expected game text-language reads were not found')
    print('Private iOS text/Options wrappers prepared; language reads:',reads)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);wrap(p.parse_args().folder)
