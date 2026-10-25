#!/usr/bin/env python3
"""Add private host boundaries without publishing generated game bodies."""
from pathlib import Path
import argparse
import re
import tempfile

WRAPPERS=('001E6AE0','0017E8A0')

def wrap(folder):
    found={address:0 for address in WRAPPERS};reads=0
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
        path.write_text(''.join(lines))
    for address,count in found.items():
        if count!=1:raise ValueError('Expected one original private wrapper body: '+address)
    header=folder/'recomp_funcs.h'
    with header.open('a') as stream:stream.write('\nunsigned ngb_ios_game_text_language(unsigned saved);\n')
    if reads<10:raise ValueError('Expected game text-language reads were not found')
    print('Private iOS text/Options wrappers prepared; language reads:',reads)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);wrap(p.parse_args().folder)
