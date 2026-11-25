"""Bind chapter-guide visibility to the actual native confirm path.

This UI adapter neither changes native branches nor exports generated code.
It runs after language variants have been merged, so both drawers are covered.
"""
from pathlib import Path
import re,sys
TOKEN=re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',re.S)
DEFINITION=re.compile(r'^void ((?:kr_|ngb_original_)?sub_001F3(?:AD0|ED0))\(void\)\s*\{',re.M)
DECLARATIONS='''
void ngb_chapter_card_update_begin(void);
void ngb_chapter_card_input_ready(void);
void ngb_chapter_card_draw_begin(unsigned mode);
void ngb_chapter_card_draw_end(void);
'''
def patch(folder):
    folder=Path(folder);outputs={};counts={('update',False):0,('draw',False):0,('update',True):0,('draw',True):0}
    for path in sorted(folder.glob('recomp_*.c')):
        source=path.read_text();masked=TOKEN.sub(lambda m:' '*len(m[0]),source)
        edits=[];wrappers=[];prototypes=[]
        for match in DEFINITION.finditer(source):
            a=match.start();brace=source.index('{',a,match.end());end=brace+1;level=1
            while level and end<len(masked):
                level+=(masked[end]=='{')-(masked[end]=='}');end+=1
            if level:raise ValueError('Unbalanced chapter-card adapter input')
            body=source[a:end];name=match[1];korean=name.startswith('kr_')
            update=name.endswith('001F3AD0');address='001F3AD0' if update else '001F3ED0'
            # Dispatch gates have no native body; their selected bodies below do.
            if 'RECOMP_ENTRY(0x'+address+'u)' not in body:continue
            role='update' if update else 'draw';counts[role,korean]+=1
            if update:
                guard=re.compile(r'if\s*\(CMP_NE\(_fa,\s*_fb\)\)\s*goto loc_001F3D50;[^\n]*')
                body,n=guard.subn(lambda m:m[0]+'\n    ngb_chapter_card_input_ready();',body)
                if n!=1:raise ValueError('Native chapter confirm guard changed; refusing a guessed readiness condition')
                at=body.index('{')+1;body=body[:at]+'\n    ngb_chapter_card_update_begin();'+body[at:]
            else:
                renamed='ngb_ui_card_body_'+name
                body=re.sub(r'\bvoid '+re.escape(name)+r'\(void\)', 'void '+renamed+'(void)',body,count=1)
                prototypes.append('void '+name+'(void);\n')
                wrappers.append('void '+name+'(void) {\n'
                    '    ngb_chapter_card_draw_begin(MEM32(0x8DB028));\n'
                    '    '+renamed+'();\n'
                    '    ngb_chapter_card_draw_end();\n}\n')
            edits.append((a,end,body))
        if edits:
            for a,end,body in reversed(edits):source=source[:a]+body+source[end:]
            outputs[path]=''.join(prototypes)+source+'\n'+''.join(wrappers)
    if counts['update',False]!=1 or counts['draw',False]!=1 or any(n>1 for n in counts.values()):
        raise ValueError('Native chapter UI adapter boundaries changed')
    # Validate everything before editing any private generated file.
    header=folder/'recomp_funcs.h';declarations=header.read_text()+DECLARATIONS
    for path,source in outputs.items():path.write_text(source)
    header.write_text(declarations)
    print('Chapter-card native input readiness and scoped guide visibility adapters installed')
if __name__=='__main__':patch(Path(sys.argv[1]))
