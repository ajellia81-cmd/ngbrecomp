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
