"""Identify the existing loading visual routes; emit addresses/flags only."""
from pathlib import Path
import struct,json,bisect
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
from capstone.x86 import X86_OP_IMM,X86_OP_MEM
b=Path('game_files/default.xbe').read_bytes();base=struct.unpack_from('<I',b,0x104)[0];n,t=struct.unpack_from('<II',b,0x11c)
sections=[struct.unpack_from('<9I20s',b,t-base+i*56) for i in range(n)]
cs=Cs(CS_ARCH_X86,CS_MODE_32);cs.detail=True;cs.skipdata=True
entries=set();items=[]
for sec in sections:
 if not sec[0]&4:continue
 for i in cs.disasm(b[sec[3]:sec[3]+sec[4]],sec[1]):
  if not i.id:continue
  if i.mnemonic=='call' and i.operands[0].type==X86_OP_IMM:entries.add(i.operands[0].imm)
  if 0x1f3000<=i.address<0x206b80:items.append(i)
entries=sorted(entries)
for role,return_site in [('loading_visual',0x20668D),('chapter_effect',0x1F5B11),('loading_effect',0x20572F)]:
 k=bisect.bisect_right(entries,return_site)-1;lo,hi=entries[k],entries[k+1]
 print('LOADING_VISUAL_REGION',json.dumps({'role':role,'begin':hex(lo),'end':hex(hi)}))
 for i in items:
  if not max(lo,return_site-512)<=i.address<min(hi,return_site+128):continue
  o=i.operands
  if i.mnemonic=='call' and o and o[0].type==X86_OP_IMM:
   print('LOADING_VISUAL_CALL',json.dumps({'role':role,'site':hex(i.address),'target':hex(o[0].imm)}))
  for op in o:
   if op.type==X86_OP_MEM and not op.mem.base and not op.mem.index and 0x8d0000<=op.mem.disp<0x900000:
    print('LOADING_VISUAL_FIELD',json.dumps({'role':role,'site':hex(i.address),'field':hex(op.mem.disp),'width':op.size}))
for i in items:
 if not any(op.type==X86_OP_IMM and op.imm==0xB8 for op in i.operands):continue
 if i.mnemonic not in ('mov','push'):continue
 k=bisect.bisect_right(entries,i.address)-1
 print('LOADING_FLOWER_ENTRY',json.dumps({'site':hex(i.address),'entry':hex(entries[k])}))
# Identify the state consumed by the existing draw-only loading routine.
for sec in sections:
 if not sec[0]&4:continue
 for i in cs.disasm(b[sec[3]:sec[3]+sec[4]],sec[1]):
  if not i.id or not 0x204540<=i.address<0x2056f0:continue
  o=i.operands
  if i.mnemonic=='call' and o and o[0].type==X86_OP_IMM:
   print('LOADING_DRAW_CALL',json.dumps({'site':hex(i.address),'target':hex(o[0].imm)}))
  for j,op in enumerate(o):
   if op.type!=X86_OP_MEM or op.mem.base or op.mem.index:continue
   at=op.mem.disp
   if not (0x8d0000<=at<0x900000 or at==0xbcdfdc):continue
   row={'site':hex(i.address),'field':hex(at),'width':op.size,'write':bool(op.access&2)}
   if i.mnemonic in ('cmp','test') and len(o)==2 and o[1-j].type==X86_OP_IMM:
    row['condition_value']=hex(o[1-j].imm&0xffffffff)
   print('LOADING_DRAW_STATE',json.dumps(row))
# Calling convention and phase predicates for safe draw-only reuse.
for sec in sections:
 if not sec[0]&4:continue
 ins=list(cs.disasm(b[sec[3]:sec[3]+sec[4]],sec[1]))
 for pos,i in enumerate(ins):
  if not i.id:continue
  o=i.operands
  if 0x205350<=i.address<0x205580 and i.mnemonic=='ret':
   print('LOADING_PROSE_RETURN',json.dumps({'site':hex(i.address),'argument_bytes':o[0].imm if o else 0}))
  if 0x206679<=i.address<0x20668D and i.mnemonic.startswith('j') and o and o[0].type==X86_OP_IMM:
   print('LOADING_DRAW_SKIP',json.dumps({'site':hex(i.address),'target':hex(o[0].imm)}))
  if not 0x204540<=i.address<0x205350:continue
  if i.mnemonic=='jmp' and o and o[0].type==X86_OP_MEM and o[0].mem.index:
   addr=o[0].mem.disp
   for table in sections:
    if table[1]<=addr<table[1]+table[4]-16:
     off=table[3]+addr-table[1]
     print('LOADING_DRAW_PHASES',json.dumps({'site':hex(i.address),'targets':[hex(v) for v in struct.unpack_from('<4I',b,off)]}))
  if i.mnemonic not in ('cmp','test','comiss','ucomiss'):continue
  operands=[]
  for op in o:
   if op.type==X86_OP_IMM:operands.append({'value':hex(op.imm&0xffffffff)})
   elif op.type==X86_OP_MEM:
    field={'memory':hex(op.mem.disp),'indexed':bool(op.mem.base or op.mem.index)}
    if not field['indexed'] and i.mnemonic.endswith('comiss'):
     for data in sections:
      if data[1]<=op.mem.disp<data[1]+data[4]-4:field['float']=struct.unpack_from('<f',b,data[3]+op.mem.disp-data[1])[0]
    operands.append(field)
   else:operands.append({'register':i.reg_name(op.reg)})
  branch=ins[pos+1] if pos+1<len(ins) else None
  print('LOADING_DRAW_GATE',json.dumps({'site':hex(i.address),'operands':operands,'branch':branch.mnemonic if branch else None,'target':hex(branch.operands[0].imm) if branch and branch.mnemonic.startswith('j') and branch.operands and branch.operands[0].type==X86_OP_IMM else None}))
# Chapter-native busy/confirm branch selectors: metadata only, no code bytes.
for sec in sections:
 if not sec[0]&4:continue
 ins=list(cs.disasm(b[sec[3]:sec[3]+sec[4]],sec[1]))
 for pos,i in enumerate(ins):
  if not i.id or not 0x1f3ed0<=i.address<0x1f5b50:continue
  o=i.operands
  if i.mnemonic=='call' and o and o[0].type==X86_OP_IMM:
   print('CHAPTER_DRAW_CALL',json.dumps({'site':hex(i.address),'target':hex(o[0].imm)}))
  if i.mnemonic not in ('cmp','test'):continue
  fields=[op.mem.disp for op in o if op.type==X86_OP_MEM and not op.mem.base and not op.mem.index and 0x8d0000<=op.mem.disp<0x8e0000]
  if not fields:continue
  nxt=ins[pos+1] if pos+1<len(ins) else None
  print('CHAPTER_DRAW_GATE',json.dumps({'site':hex(i.address),'fields':[hex(x) for x in fields],'values':[op.imm for op in o if op.type==X86_OP_IMM],'branch':nxt.mnemonic if nxt else None,'target':hex(nxt.operands[0].imm) if nxt and nxt.mnemonic.startswith('j') and nxt.operands and nxt.operands[0].type==X86_OP_IMM else None}))
# Register-based chapter selectors and the existing flying-flower helper.
for sec in sections:
 if not sec[0]&4:continue
 ins=list(cs.disasm(b[sec[3]:sec[3]+sec[4]],sec[1]))
 for pos,i in enumerate(ins):
  if not i.id or not (0x1f5800<=i.address<0x1f5b50 or 0x1f97f0<=i.address<0x1f9a00):continue
  o=i.operands
  for op in o:
   if op.type==X86_OP_MEM and not op.mem.base and not op.mem.index and 0x8d0000<=op.mem.disp<0x8e0000:
    print('CHAPTER_FLOWER_FIELD',json.dumps({'site':hex(i.address),'field':hex(op.mem.disp),'write':bool(op.access&2),'register':i.reg_name(o[0].reg) if o[0].type==1 else None}))
  if i.mnemonic not in ('cmp','test','comiss','ucomiss'):continue
  nxt=ins[pos+1] if pos+1<len(ins) else None
  vals=[]
  for op in o:
   if op.type==X86_OP_IMM:vals.append({'value':op.imm})
   elif op.type==1:vals.append({'register':i.reg_name(op.reg)})
   elif op.type==X86_OP_MEM:vals.append({'field':hex(op.mem.disp),'indexed':bool(op.mem.base or op.mem.index)})
  print('CHAPTER_FLOWER_GATE',json.dumps({'site':hex(i.address),'operands':vals,'branch':nxt.mnemonic if nxt else None,'target':hex(nxt.operands[0].imm) if nxt and nxt.mnemonic.startswith('j') and nxt.operands and nxt.operands[0].type==X86_OP_IMM else None}))
