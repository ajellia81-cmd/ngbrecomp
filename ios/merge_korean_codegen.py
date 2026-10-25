#!/usr/bin/env python3
"""Merge only changed/new private Korean functions into the original tree.

The original generated bodies are retained. Address dispatch selects variants
only for a Korean boot; font wrappers retain their handwritten host hooks.
Never commit the output of this script.
"""
import argparse
import json
import re
from pathlib import Path

DEFINITION = re.compile(r'^void (sub_[0-9A-Fa-f]{8})\(void\)\s*\{', re.M)
TOKENS = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)


def functions(folder):
    result = {}
    for path in sorted(folder.glob('recomp_[0-9]*.c')):
        source = path.read_text()
        masked = TOKENS.sub(lambda m: ' ' * len(m[0]), source)
        for match in DEFINITION.finditer(source):
            start = match.start()
            brace = source.index('{', match.start(), match.end())
            depth = 1
            end = brace + 1
            while depth and end < len(masked):
                depth += (masked[end] == '{') - (masked[end] == '}')
                end += 1
            if depth:
                raise ValueError('Unbalanced generated function')
            result[match[1]] = source[start:end]
    return result


def normalized(body):
    return re.sub(r'\s+', '', re.sub(r'/\*.*?\*/|//[^\n]*', '', body, flags=re.S))


def restore_wrapper_stack(name, body):
    # These two pinned patch wrappers push flags solely for later restoration;
    # they never inspect that stack word. The lifter retains caller conditions
    # in caller-local snapshots, which C callees cannot overwrite. Reserve the
    # real guest stack word so PUSHAD/locals/return addresses keep their offsets.
    # This is NOT a general PUSHFD/POPFD implementation or an EFLAGS emulator.
    pairs = {'sub_00E02E50': ('00E02E50','00E02F97'),
             'sub_00E03430': ('00E03430','00E03459')}
    if name not in pairs: return body
    start,end=pairs[name]
    for instruction,address,replacement in (
        ('pushfd',start,'esp -= 4; /* unobserved saved flags; caller snapshots retained */'),
        ('popfd',end,'esp += 4; /* discard saved flags word; caller snapshots retained */')):
        old=f'RECOMP_UNIMPL("{instruction}", 0x{address}u); /* TODO: {instruction} */'
        if body.count(old)!=1:
            raise ValueError('Pinned Korean flags-wrapper shape changed')
        body=body.replace(old,replacement)
    return body


def repair_font_trampolines(bodies):
    # The pinned patch uses PUSH continuation; RET as a jump. A normal lifted
    # RET would return to the host caller and skip the rest of glyph setup.
    # Rejoin the existing caller body so its live frame and loop stay intact.
    for helper, target in [('00DCA400', '0021A23A'), ('00DCA600', '0021A4B6')]:
        key='sub_'+helper
        if key not in bodies:
            raise ValueError('Pinned Korean font trampoline missing')
        body=bodies[key]
        old='PUSH32(esp, 0x'+target.lstrip('0')+');\n    esp += 4; return; /* ret */'
        if body.count(old)!=1:
            raise ValueError('Pinned Korean font trampoline shape changed')
        has_frame=bool(re.search(r'uint32_t ebp\s*=',body))
        body=body.replace(old,('g_seh_ebp = ebp; ' if has_frame else '')+
                          'return; /* resume in the patched caller */')
        bodies[key]=body
        pattern=re.compile(r'g_seh_ebp = ebp; '+key+r'\(\); return; /\* tail jmp 0x'+helper+r' \*/')
        count=0
        for caller,source in list(bodies.items()):
            if 'loc_'+target+':' not in source: continue
            source,n=pattern.subn('g_seh_ebp = ebp; '+key+'(); '+
                ('ebp = g_seh_ebp; ' if has_frame else '')+'goto loc_'+target+';',source)
            if n: bodies[caller]=source; count+=n
        expected=2 if helper=='00DCA400' else 1
        if count!=expected:
            raise ValueError('Pinned Korean font trampoline caller changed')


HOST_WRAPPERS={'sub_001E6D30','sub_001E86A0','sub_001E6AE0','sub_0017E8A0'}

def route_direct_entries(original, selected):
    # RECOMP_ABI_CALL invokes its C symbol directly. The manual lookup only
    # handles indirect calls, so every generated selected entry needs a gate.
    routed=set()
    for path in sorted(original.glob('recomp_[0-9]*.c')):
        source=path.read_text()
        additions=[]
        for name in sorted(set(selected)-HOST_WRAPPERS):
            pattern=re.compile(r'\bvoid '+name+r'\(void\)\s*\{')
            source,n=pattern.subn('void ngb_original_'+name+'(void) {',source)
            if n:
                if n!=1 or name in routed: raise ValueError('Duplicate private language entry')
                routed.add(name)
                additions.append('void '+name+'(void) {\n'
                    '#ifdef NGB_IOS_KOREAN\n'
                    '    extern void kr_'+name+'(void);\n'
                    '    extern int ngb_korean_active(void);\n'
                    '    if (ngb_korean_active()) { kr_'+name+'(); return; }\n'
                    '#endif\n'
                    '    ngb_original_'+name+'();\n}')
        if additions: path.write_text(source+'\n'+'\n'.join(additions)+'\n')
    return routed


def merge(original, translated, missing_seeds=None):
    before, after = functions(original), functions(translated)
    repair_font_trampolines(after)
    changed = {name: body for name, body in after.items()
               if name in before and normalized(body) != normalized(before[name])}
    # Executable XBE sections can also hold text data. Do not import functions
    # found by a linear sweep of those bytes unless reachable from real changed
    # code. The three handwritten entries contain verified patched branches.
    roots=set(changed) | {'sub_001E6D30','sub_001E86A0','sub_0021C660','sub_001E6AE0','sub_0017E8A0'}
    old_symbols=set(re.findall(r'void (sub_[0-9A-Fa-f]{8})\(', (original/'recomp_funcs.h').read_text()))
    selected={}; pending=list(roots); missing=set()
    while pending:
        name=pending.pop()
        if name in selected: continue
        body=after.get(name)
        if body is None:
            if name not in old_symbols: missing.add(int(name[4:],16))
            continue
        if name in before and name not in changed and name not in roots: continue
        selected[name]=body
        code=TOKENS.sub('',body)
        # Continuations can appear as function-pointer values, not just calls.
        targets=set(re.findall(r'\bsub_([0-9A-Fa-f]{8})\b',code))
        targets.update(re.findall(r'RECOMP_(?:CALL|TAIL)\(0x([0-9A-Fa-f]{8})u?\)',code))
        for address in targets:
            target='sub_'+address.upper()
            if target != name: pending.append(target)
    if missing:
        if missing_seeds:
            previous=json.loads(missing_seeds.read_text()) if missing_seeds.exists() else []
            missing.update(int(item['start'],16) for item in previous)
            missing_seeds.write_text(json.dumps([{'start':f'0x{address:08X}',
                'static_branch':True,
                'note':'Static Korean patch continuation; not runtime-observed'} for address in sorted(missing)]))
        print('Korean native continuations need actual translation:',len(missing))
        return False
    if not selected or not any(int(name[4:], 16) >= 0x00DF0000 for name in selected):
        raise ValueError('Korean font/helper code was not generated')
    # Reject new untranslated instructions. Existing unreachable baseline
    # decoding artefacts are not introduced by the Korean adaptation.
    baseline_unimplemented=set(re.findall(r'RECOMP_UNIMPL\("[^\"]*", (0x[0-9A-F]+)u\)',
        '\n'.join(before.values())))
    for name,body in selected.items():
        body=restore_wrapper_stack(name,body)
        unimplemented=set(re.findall(r'RECOMP_UNIMPL\("[^\"]*", (0x[0-9A-F]+)u\)',body))
        if unimplemented-baseline_unimplemented:
            raise ValueError('New Korean instruction remains untranslated; refusing an incomplete font helper')
        selected[name]=body
    output = ['/* PRIVATE generated language variants: never commit. */',
              '#ifdef NGB_IOS_KOREAN', '#define RECOMP_GENERATED_CODE', '#include "recomp_funcs.h"',
              '#include "korean.h"', '#include <stddef.h>', '#include <math.h>']
    output.extend('void kr_' + name + '(void);' for name in sorted(selected))
    for name, body in sorted(selected.items()):
        # A few generator-internal continuations call C functions directly.
        # Route those to the matching variant while retaining font host hooks.
        def rename(match):
            target = match[0]
            return 'kr_' + target if target in selected and int(target[4:],16) not in {0x001E6D30,0x001E86A0,0x001E6AE0,0x0017E8A0} else target
        body = re.sub(r'\bsub_[0-9A-Fa-f]{8}\b', rename, body)
        if name in {'sub_001E6D30','sub_001E86A0','sub_001E6AE0','sub_0017E8A0'}:
            body = body.replace('void ' + name + '(void)', 'void kr_' + name + '(void)', 1)
        output.append(body)
        for macro in ['fp_push', 'fp_pop', 'fp_top', 'fp_st', 'fp_st1']:
            output.append('#undef ' + macro)
    output.extend(['typedef struct { unsigned address; ngb_korean_fn function; } KoreanEntry;',
                   'static const KoreanEntry entries[] = {'])
    for name in sorted(selected):
        output.append('{0x' + name[4:] + 'u, kr_' + name + '},')
    output.extend(['};', 'ngb_korean_fn ngb_korean_lookup_raw(unsigned address) {',
                  'if (!ngb_korean_active()) return NULL;',
                  'size_t lo=0, hi=sizeof entries / sizeof entries[0];',
                  'while (lo < hi) { size_t m=lo+(hi-lo)/2;',
                  'if (entries[m].address < address) lo=m+1;',
                  'else if (entries[m].address > address) hi=m;',
                  'else return entries[m].function; } return NULL; }',
                  'ngb_korean_fn ngb_korean_lookup(unsigned address) {',
                  'if (address == 0x001E6D30u || address == 0x001E86A0u || address == 0x001E6AE0u || address == 0x0017E8A0u) return NULL;',
                  'return ngb_korean_lookup_raw(address); }', '#endif'])
    routed=route_direct_entries(original,selected)
    print('Private Korean direct-call entry gates:',len(routed))
    (original / 'recomp_korean.c').write_text('\n'.join(output) + '\n')
    # Same register ABI; the patched executable adds an executable helper range.
    # Original image dispatch is unchanged; variants use the manual lookup first.
    (original / 'recomp_types.h').write_bytes((translated / 'recomp_types.h').read_bytes())
    print('Private Korean changed/new function variants:', len(selected))
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--translated', type=Path, required=True)
    parser.add_argument('--missing-seeds', type=Path)
    args = parser.parse_args()
    raise SystemExit(0 if merge(args.original, args.translated,args.missing_seeds) else 2)
