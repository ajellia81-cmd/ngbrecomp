#!/usr/bin/env python3
from pathlib import Path
import re,sys

def wrap(folder):
    counts={a:0 for a in ('001E6AE0','0017E8A0','00232D40')};reads=0
    for p in folder.glob('recomp_*.c'):
        s=p.read_text()
        for a in counts:
            s,n=re.subn(r'\bvoid sub_'+a+r'\(void\)\s*\{','void ngb_base_'+a+'(void) {',s);counts[a]+=n
        out=[]
        for line in s.splitlines(keepends=True):
            if 'ngb_ios_game_text_language' not in line:
                line,n=re.subn(r'MEM(?:8|32)\(0xDB9890\)(?!\s*=(?!=))',lambda m:'ngb_ios_game_text_language('+m[0]+')',line);reads+=n
            out.append(line)
        p.write_text(''.join(out))
    if any(n!=1 for n in counts.values()) or reads<10:raise ValueError('Original language hook boundaries changed')
    with (folder/'recomp_funcs.h').open('a') as f:f.write('\nunsigned ngb_ios_game_text_language(unsigned saved);\n')
    print('Language-only generated adapters installed')
if __name__=='__main__':wrap(Path(sys.argv[1]))
