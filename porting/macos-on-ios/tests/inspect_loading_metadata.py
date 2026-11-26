"""Report only the native loading-caption identity and pointer columns."""
from pathlib import Path
import struct,json,re
b=Path('game_files/default.xbe').read_bytes();base=struct.unpack_from('<I',b,0x104)[0];n,t=struct.unpack_from('<II',b,0x11c)
sections=[struct.unpack_from('<9I20s',b,t-base+i*56) for i in range(n)]
def offset(va,length):
 for s in sections:
  if s[1]<=va and va+length<=s[1]+s[4]:return s[3]+va-s[1]
 if base<=va and va+length<=base+0x1000:return va-base
 return None
found=0
for ident in range(256):
 row=0x4316BC+ident*12;at=offset(row,12)
 if at is None:continue
 actual,jp,en=struct.unpack_from('<3I',b,at)
 if actual!=ident:continue
 text=offset(en,80)
 if text is None:continue
 caption=b[text:text+80].split(b'\0',1)[0]
 if re.fullmatch(rb'(?:now[ \t]+)?loading[ .]*',caption,re.I):
  print('LOADING_CAPTION_ROW',json.dumps({'id':ident,'row':hex(row),'jp':hex(jp),'english':hex(en),'length':len(caption)}));found+=1
if not found:raise ValueError('No verified native loading caption in the pinned footer table')
