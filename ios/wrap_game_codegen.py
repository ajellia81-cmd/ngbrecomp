#!/usr/bin/env python3
"""Add private host boundaries without publishing generated game bodies."""
from pathlib import Path
import argparse
import re
import tempfile

WRAPPERS=('001E6AE0','0017E8A0','00232D40')
CACHE_WAIT='loc_0031C750: ;\n    PUSH32(esp, 0x32);'

def adapt_cache_wait(text):
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
    cache_waits=0
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
        path.write_text(adapt_cache_wait(instrument_movie(result)))
    for address,count in found.items():
        if count!=1:raise ValueError('Expected one original private wrapper body: '+address)
    if any(count!=1 for count in probes.values()):
        raise ValueError('Expected one original movie diagnostic boundary per stage')
    if cache_waits!=1:raise ValueError('Expected one original cache-worker packet wait')
    header=folder/'recomp_funcs.h'
    with header.open('a') as stream:stream.write('\nunsigned ngb_ios_game_text_language(unsigned saved);\n')
    with header.open('a') as stream:stream.write('\nunsigned ngb_ios_cache_copy_delay(unsigned milliseconds,unsigned state);\n')
    with header.open('a') as stream:stream.write('\n#ifdef NGB_IOS_MOVIE_DIAGNOSTICS\nvoid ngb_ios_movie_probe(unsigned stage);\n#endif\n')
    if reads<10:raise ValueError('Expected game text-language reads were not found')
    print('Private iOS text/Options wrappers prepared; language reads:',reads)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);wrap(p.parse_args().folder)
