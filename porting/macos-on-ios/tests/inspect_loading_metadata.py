"""Report native chapter animation state/threshold metadata, never code or assets."""
from pathlib import Path
import struct,json
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
from capstone.x86 import X86_OP_IMM,X86_OP_MEM,X86_OP_REG
b=Path('game_files/default.xbe').read_bytes();base=struct.unpack_from('<I',b,0x104)[0];n,t=struct.unpack_from('<II',b,0x11c)
sections=[struct.unpack_from('<9I20s',b,t-base+i*56) for i in range(n)]
cs=Cs(CS_ARCH_X86,CS_MODE_32);cs.detail=True;cs.skipdata=True
for s in sections:
 if not s[0]&4:continue
 lo=max(s[1],0x1f3ad0);hi=min(s[1]+s[4],0x1f4d20)
 if lo>=hi:continue
 raw=b[s[3]+lo-s[1]:s[3]+hi-s[1]];ins=list(cs.disasm(raw,lo));origins={}
 for k,i in enumerate(ins):
  if not i.id:continue
  o=i.operands
  if i.mnemonic.startswith('mov') and len(o)==2 and o[0].type==X86_OP_REG:
   a=o[1];origins[i.reg_name(o[0].reg)]={'field':hex(a.mem.disp),'base':i.reg_name(a.mem.base),'index':i.reg_name(a.mem.index)} if a.type==X86_OP_MEM else {'value':a.imm} if a.type==X86_OP_IMM else {'register':i.reg_name(a.reg)} if a.type==X86_OP_REG else {}
  fields=[{'field':hex(op.mem.disp),'write':bool(op.access&2)} for op in o if op.type==X86_OP_MEM and not op.mem.base and not op.mem.index and 0x8d0000<=op.mem.disp<0x8e0000]
  if fields:print('CHAPTER_TIMELINE_STATE',json.dumps({'site':hex(i.address),'fields':fields,'values':[op.imm for op in o if op.type==X86_OP_IMM]}))
  if i.mnemonic in ('cmp','test','comiss','ucomiss'):
   nxt=ins[k+1] if k+1<len(ins) else None
   print('CHAPTER_TIMELINE_THRESHOLD',json.dumps({'site':hex(i.address),'values':[op.imm for op in o if op.type==X86_OP_IMM],'sources':[origins.get(i.reg_name(op.reg),{}) for op in o if op.type==X86_OP_REG],'branch':nxt.mnemonic if nxt else None,'target':hex(nxt.operands[0].imm) if nxt and nxt.mnemonic.startswith('j') and nxt.operands[0].type==X86_OP_IMM else None}))
