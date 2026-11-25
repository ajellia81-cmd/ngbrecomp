"""Adapt native chapter readiness and the exact footer route, without exporting game code."""
from pathlib import Path
import re,sys
TOKEN=re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',re.S)
ROLES={'001F3AD0':'update','001F3ED0':'draw','002DF930':'start','001F5E40':'footer'}
DEFINITION=re.compile(r'^void ((?:kr_|ngb_original_)?sub_('+ '|'.join(ROLES) +r'))\(void\)\s*\{',re.M)
DECLARATIONS='''
void ngb_chapter_card_update_begin(void);
void ngb_chapter_card_input_ready(void);
void ngb_chapter_card_draw_begin(unsigned mode);
void ngb_chapter_card_draw_end(void);
void ngb_chapter_card_start_begin(void);
void ngb_chapter_card_start_ready(void);
void ngb_chapter_card_start_end(void);
void ngb_chapter_card_footer_begin(unsigned caller,unsigned mode);
void ngb_chapter_card_footer_end(void);
'''
def patch(folder):
    folder=Path(folder);outputs={};counts={(role,k):0 for role in ROLES.values() for k in (False,True)}
    for path in sorted(folder.glob('recomp_*.c')):
        source=path.read_text();masked=TOKEN.sub(lambda m:' '*len(m[0]),source)
        edits=[];wrappers=[];prototypes=[]
        for match in DEFINITION.finditer(source):
            a=match.start();brace=source.index('{',a,match.end());end=brace+1;level=1
            while level and end<len(masked):
                level+=(masked[end]=='{')-(masked[end]=='}');end+=1
            if level:raise ValueError('Unbalanced chapter-card adapter input')
            body=source[a:end];name,address=match[1],match[2];korean=name.startswith('kr_')
            if 'RECOMP_ENTRY(0x'+address+'u)' not in body:continue
            role=ROLES[address];counts[role,korean]+=1
            if role=='update':
                guard=re.compile(r'if\s*\(CMP_NE\(_fa,\s*_fb\)\)\s*goto loc_001F3D50;[^\n]*')
                body,n=guard.subn(lambda m:m[0]+'\n    ngb_chapter_card_input_ready();',body)
                if n!=1:raise ValueError('Native common-card confirm guard changed')
                at=body.index('{')+1;body=body[:at]+'\n    ngb_chapter_card_update_begin();'+body[at:]
            else:
                if role=='start':
                    # This native call returns after preparation, immediately
                    # before the chapter's separate A/X/Start wait loop.
                    gate=re.compile(r'(?m)^[ \t]*PUSH32\(esp,\s*0x002DFCB5u\);\s*RECOMP_ABI_CALL\(0x002ECC50u,\s*(?:kr_)?sub_002ECC50\);[^\n]*')
                    body,n=gate.subn(lambda m:m[0]+'\n    ngb_chapter_card_start_ready();',body)
                    if n!=1:raise ValueError('Native chapter-start preparation boundary changed')
                renamed='ngb_ui_card_body_'+name
                body=re.sub(r'\bvoid '+re.escape(name)+r'\(void\)', 'void '+renamed+'(void)',body,count=1)
                prototypes.append('void '+name+'(void);\n')
                begin={'draw':'ngb_chapter_card_draw_begin(MEM32(0x8DB028));',
                       'start':'ngb_chapter_card_start_begin();',
                       'footer':'ngb_chapter_card_footer_begin(MEM32(esp),MEM32(0x8DB028));'}[role]
                end_call={'draw':'ngb_chapter_card_draw_end();','start':'ngb_chapter_card_start_end();',
                          'footer':'ngb_chapter_card_footer_end();'}[role]
                wrappers.append('void '+name+'(void) {\n    '+begin+'\n    '+renamed+'();\n    '+end_call+'\n}\n')
            edits.append((a,end,body))
        if edits:
            for a,end,body in reversed(edits):source=source[:a]+body+source[end:]
            outputs[path]=''.join(prototypes)+source+'\n'+''.join(wrappers)
    if any(counts[role,False]!=1 for role in ROLES.values()) or any(n>1 for n in counts.values()):
        raise ValueError('Native chapter UI adapter boundaries changed')
    header=folder/'recomp_funcs.h';declarations=header.read_text()+DECLARATIONS
    for path,source in outputs.items():path.write_text(source)
    header.write_text(declarations)
    print('Chapter common/start readiness and exact footer-route adapters installed')
if __name__=='__main__':patch(Path(sys.argv[1]))
