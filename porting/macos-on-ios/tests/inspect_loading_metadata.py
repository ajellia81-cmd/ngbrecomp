"""Bounded chapter-card input/display predicates, never game code or bytes."""
from pathlib import Path
import struct,json,bisect
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
from capstone.x86 import X86_OP_IMM,X86_OP_MEM,X86_OP_REG
b=Path('game_files/default.xbe').read_bytes();base=struct.unpack_from('<I',b,0x104)[0];n,t=struct.unpack_from('<II',b,0x11c)
sections=[struct.unpack_from('<9I20s',b,t-base+i*56) for i in range(n)]
cs=Cs(CS_ARCH_X86,CS_MODE_32);cs.detail=True;cs.skipdata=True
items=[];entries=set()
for sec in sections:
 if not sec[0]&4:continue
 for i in cs.disasm(b[sec[3]:sec[3]+sec[4]],sec[1]):
  if not i.id:continue
  if i.mnemonic=='call' and i.operands[0].type==X86_OP_IMM:entries.add(i.operands[0].imm)
  if 0x1f3000<=i.address<0x1f6000:items.append(i)
entries=sorted(entries);k=bisect.bisect_right(entries,0x1F5AF6)-1
ranges=[('chapter_card_draw',entries[k],entries[k+1]),('chapter_card_update_candidate',entries[k-1],entries[k])]
alias={v:r for r,ls in [('eax',('ax','al','ah')),('ebx',('bx','bl','bh')),('ecx',('cx','cl','ch')),('edx',('dx','dl','dh')),('esi',('si',)),('edi',('di',))] for v in ls}
for role,lo,hi in ranges:
 print('CARD_CONDITION_REGION',json.dumps({'role':role,'begin':hex(lo),'end':hex(hi)}))
 regs={};flags=('entry',);flag_site=lo
 def short(v):return v if len(v)<360 else v[:340]+'...'
 def value(op,deref=True):
  if op.type==X86_OP_IMM:return hex(op.imm&0xffffffff)
  if op.type==X86_OP_REG:
   r=i.reg_name(op.reg);v=regs.get(alias.get(r,r),alias.get(r,r))
   if r in ('al','bl','cl','dl'):v='low8('+v+')'
   if r in ('ah','bh','ch','dh'):v='high8('+v+')'
   return short(v)
  if op.type==X86_OP_MEM:
   m=op.mem;v=hex(m.disp&0xffffffff)
   if m.base:v+='+'+regs.get(i.reg_name(m.base),i.reg_name(m.base))
   if m.index:v+='+'+regs.get(i.reg_name(m.index),i.reg_name(m.index))+'*'+str(m.scale)
   return short(('mem'+str(op.size*8)+'['+v+']') if deref else v)
  return 'unknown'
 def assign(op,v):
  if op.type!=X86_OP_REG:return
  r=i.reg_name(op.reg);key=alias.get(r,r)
  if r!=key:v='set_'+r+'('+regs.get(key,key)+','+v+')'
  regs[key]=short(v)
 for i in items:
  if not lo<=i.address<hi:continue
  o=i.operands;m=i.mnemonic
  if m in ('mov','movzx','movsx','movss') and len(o)==2:assign(o[0],value(o[1]))
  elif m=='lea' and len(o)==2:assign(o[0],value(o[1],False))
  elif m in ('cmp','test','comiss','ucomiss') and len(o)==2:flags=(m,value(o[0]),value(o[1]));flag_site=i.address
  elif m.startswith('set') and len(o)==1:assign(o[0],m[3:]+'('+','.join(flags)+')')
  elif m in ('and','or','xor','add','sub','shl','shr','sar','addss','subss','mulss','divss') and len(o)==2:
   a,c=value(o[0]),value(o[1]);v='0' if m=='xor' and a==c else m+'('+a+','+c+')';assign(o[0],v)
   if not m.endswith('ss'):flags=(m,a,c);flag_site=i.address
  elif m=='imul' and len(o) in (2,3):assign(o[0],'mul('+value(o[-2])+','+value(o[-1])+')');flags=('mul',);flag_site=i.address
  elif m in ('inc','dec','neg','not') and len(o)==1:
   old=value(o[0]);assign(o[0],m+'('+old+')');flags=(m,old);flag_site=i.address
  elif m.startswith('j') and m!='jmp' and o and o[0].type==X86_OP_IMM:
   print('CARD_INPUT_DISPLAY_PREDICATE',json.dumps({'role':role,'site':hex(i.address),'condition':m,'flags_site':hex(flag_site),'predicate':flags,'target':hex(o[0].imm)}))
  elif m.startswith('cmov') and len(o)==2:assign(o[0],m[4:]+'('+','.join(flags)+')?'+value(o[1])+':'+value(o[0]))
  elif m=='call':
   regs['eax']='check_result@'+hex(i.address);regs['ecx']='ecx_after_call';regs['edx']='edx_after_call'
