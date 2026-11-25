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
  if lo<=i.address<hi or 0x21a760<=i.address<0x21a7d0 or 0x16d2b0<=i.address<0x16d340:instructions.append(i)
entries=sorted(entries)
# Only the loading controller's wait and guide gates, not a code listing.
for lower,upper in [(0x2041E0,0x204290),(0x205350,0x205580)]:
 part=[i for i in instructions if lower<=i.address<upper]
 for ix,i in enumerate(part):
  if i.mnemonic not in ('cmp','test','comiss','ucomiss'):continue
  operands=[]
  for op in i.operands:
   if op.type==X86_OP_IMM:operands.append({'constant':hex(op.imm&0xffffffff)})
   elif op.type==X86_OP_MEM:
    d=op.mem.disp
    item={'global':hex(d)} if not op.mem.base and not op.mem.index else {'indexed_base':hex(d)}
    for s in sections:
     if not op.mem.base and not op.mem.index and s[1]<=d<s[1]+s[4]-4:
      value=struct.unpack_from('<I',b,s[3]+d-s[1])[0];item['initial_u32']=value
      if i.mnemonic.endswith('comiss'):item['initial_float']=struct.unpack('<f',struct.pack('<I',value))[0]
    operands.append(item)
   else:operands.append({'register':i.reg_name(op.reg)})
  nxt=part[ix+1] if ix+1<len(part) else None
  branch={'condition':nxt.mnemonic,'target':hex(nxt.operands[0].imm)} if nxt and nxt.mnemonic.startswith('j') and nxt.operands and nxt.operands[0].type==X86_OP_IMM else None
  print('LOADING_GATE',json.dumps({'site':hex(i.address),'operands':operands,'branch':branch}))
# Fixed controller data relevant to a loading-screen minimum wait.
for addr in (0x4148C8,0x4148EC,0x414EF4,0x415460,0x415470):
 for s in sections:
  if s[1]<=addr<s[1]+s[4]-4:
   v=b[s[3]+addr-s[1]:s[3]+addr-s[1]+4]
   print('LOADING_CONSTANT',json.dumps({'address':hex(addr),'u32':struct.unpack('<I',v)[0],'f32':struct.unpack('<f',v)[0]}))
# Resolve only the controller's opening predicates symbolically. No guest code
# or instruction strings leave the runner; output is loading-condition metadata.
regs={};flags=('unknown','unknown','unknown');last_flags=None
alias={'al':'eax','ah':'eax','ax':'eax','bl':'ebx','bh':'ebx','bx':'ebx','cl':'ecx','ch':'ecx','cx':'ecx','dl':'edx','dh':'edx','dx':'edx','si':'esi','di':'edi'}
def regname(op):return i.reg_name(op.reg)
def value(op):
 if op.type==X86_OP_IMM:return hex(op.imm&0xffffffff)
 if op.type==1:
  r=regname(op);v=regs.get(alias.get(r,r),r)
  return ('low8('+v+')') if r in ('al','bl','cl','dl') else v
 if op.type==X86_OP_MEM:
  m=op.mem;v=hex(m.disp&0xffffffff)
  if m.base:v+='+'+regs.get(i.reg_name(m.base),i.reg_name(m.base))
  if m.index:v+='+'+regs.get(i.reg_name(m.index),i.reg_name(m.index))+'*'+str(m.scale)
  return 'memory'+str(op.size*8)+'['+v+']'
 return 'unknown'
def assign(op,v):
 if op.type!=1:return
 r=regname(op);regs[alias.get(r,r)]=v
for i in instructions:
 if not (0x2041e0<=i.address<0x204520 or 0x21a760<=i.address<0x21a7d0 or 0x16d2b0<=i.address<0x16d340):continue
 if i.address in (0x2041e0,0x21a760,0x16d2b0):regs={};last_flags=None;flags=('unknown','unknown','unknown')
 op=i.operands;m=i.mnemonic
 if m in ('mov','movzx','movsx','movss') and len(op)==2:assign(op[0],value(op[1]))
 elif m in ('cmp','test','comiss','ucomiss') and len(op)==2:flags=(m,value(op[0]),value(op[1]));last_flags=i.address
 elif m.startswith('set') and len(op)==1:assign(op[0],m[3:]+'('+','.join(flags)+')')
 elif m in ('and','or','xor','add','sub','shl','shr','sar') and len(op)==2:
  a,v=value(op[0]),value(op[1]);assign(op[0],'0' if m=='xor' and a==v else m+'('+a+','+v+')')
 elif m in ('neg','not','inc','dec') and len(op)==1:assign(op[0],m+'('+value(op[0])+')')
 elif m.startswith('j') and m!='jmp':
  print('LOADING_PREDICATE',json.dumps({'site':hex(i.address),'condition':m,'flags_source':hex(last_flags) if last_flags else None,'predicate':flags,'target':hex(op[0].imm) if op[0].type==X86_OP_IMM else 'indirect'}))
 elif m.startswith('cmov') and len(op)==2:assign(op[0],m[4:]+'('+','.join(flags)+')?'+value(op[1])+':'+value(op[0]))
 elif m=='jmp' and op[0].type==X86_OP_MEM and op[0].mem.index:
  addr=op[0].mem.disp
  for sec in sections:
   if sec[1]<=addr<sec[1]+sec[4]-20:
    vals=struct.unpack_from('<5I',b,sec[3]+addr-sec[1]);print('LOADING_PHASE_BRANCHES',json.dumps({'site':hex(i.address),'targets':[hex(v) for v in vals]}))
 elif m=='ret':
  print('LOADING_RETURN_PREDICATE',json.dumps({'site':hex(i.address),'return':regs.get('eax','eax')}))
 elif m=='call':
  regs['eax']='check_at_'+hex(i.address)+'('+','.join(regs.get(r,r) for r in ('eax','ecx','edx'))+')'
