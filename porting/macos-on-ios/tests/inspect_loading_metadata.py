"""Print bounded semantic metadata; never emit guest bytes or disassembly."""
from pathlib import Path
import struct,json
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
from capstone.x86 import X86_OP_IMM,X86_OP_MEM
b=Path('game_files/default.xbe').read_bytes();base=struct.unpack_from('<I',b,0x104)[0];n,table=struct.unpack_from('<II',b,0x11c)
sections=[struct.unpack_from('<9I20s',b,table-base+i*56) for i in range(n)]
cs=Cs(CS_ARCH_X86,CS_MODE_32);cs.detail=True;cs.skipdata=True
lo,hi=0x204000,0x206B80
instructions=[];entries=set()
for s in sections:
 if not s[0]&4:continue
 for i in cs.disasm(b[s[3]:s[3]+s[4]],s[1]):
  if not i.id:continue
  if i.mnemonic=='call' and i.operands[0].type==X86_OP_IMM:
   dest=i.operands[0].imm
   if lo<=dest<hi:entries.add(dest)
  if lo<=i.address<hi:instructions.append(i)
entries=sorted(entries)
print('CHAPTER_FUNCTION_ENTRIES',json.dumps([hex(x) for x in entries]))
for k,start in enumerate(entries):
 end=entries[k+1] if k+1<len(entries) else hi
 part=[i for i in instructions if start<=i.address<end]
 calls=[];globals=set();tests=[];writes=[]
 for ix,i in enumerate(part):
  ops=i.operands
  if i.mnemonic=='call' and ops[0].type==X86_OP_IMM:calls.append(hex(ops[0].imm))
  for op in ops:
   if op.type==X86_OP_MEM and not op.mem.base and not op.mem.index and op.mem.disp>0x400000:globals.add(op.mem.disp)
  if i.mnemonic in ('cmp','test') and len(ops)==2 and ops[1].type==X86_OP_IMM:
   source=None
   if ops[0].type==X86_OP_MEM and not ops[0].mem.base and not ops[0].mem.index:source=ops[0].mem.disp
   elif ops[0].type==1:
    for prev in reversed(part[max(0,ix-5):ix]):
     po=prev.operands
     if len(po)==2 and po[0].type==1 and po[0].reg==ops[0].reg:
      if prev.mnemonic.startswith('mov') and po[1].type==X86_OP_MEM and not po[1].mem.base and not po[1].mem.index:source=po[1].mem.disp
      break
   if source and source>0x400000:tests.append({'site':hex(i.address),'global':hex(source),'value':hex(ops[1].imm&0xffffffff)})
  if i.mnemonic=='mov' and len(ops)==2 and ops[0].type==X86_OP_MEM and not ops[0].mem.base and not ops[0].mem.index and ops[0].mem.disp>0x400000 and ops[1].type==X86_OP_IMM:
   writes.append({'site':hex(i.address),'global':hex(ops[0].mem.disp),'value':hex(ops[1].imm&0xffffffff)})
 print('CHAPTER_METADATA',json.dumps({'entry':hex(start),'end':hex(end),'calls':list(dict.fromkeys(calls)),'globals':[hex(x) for x in sorted(globals)],'test_candidates':tests,'constant_writes':writes}))
