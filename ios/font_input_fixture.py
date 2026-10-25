"""Generate a tiny synthetic XDVDFS, with no game files, to test extraction."""
from pathlib import Path
import hashlib
import json
import struct
import sys
root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
image=bytearray(48*2048)
image[32*2048:32*2048+20]=b'MICROSOFT*XBOX*MEDIA'
struct.pack_into('<II',image,32*2048+20,40,2048)
def directory(sector,entries):
    offsets=[];cursor=0
    for name,lba,size,folder in entries:
        offsets.append(cursor);cursor=(cursor+14+len(name)+3)&~3
    for i,(name,lba,size,folder) in enumerate(entries):
        offset=sector*2048+offsets[i]
        struct.pack_into('<HHIIBB',image,offset,0,offsets[i+1]//4 if i+1<len(entries) else 0,lba,size,16 if folder else 0,len(name))
        image[offset+14:offset+14+len(name)]=name.encode('ascii')
data={'Media/font.ttf':b'\0\1\0\0synthetic font','Media/font.xpr':b'XPR0synthetic latin','Media/Fonts/kfont.xpr':b'XPR0synthetic korean'}
directory(40,[('mEdIa',41,2048,True)])
directory(41,[('FONT.TTF',43,len(data['Media/font.ttf']),False),('font.xpr',44,len(data['Media/font.xpr']),False),('Fonts',42,2048,True)])
directory(42,[('kfont.xpr',45,len(data['Media/Fonts/kfont.xpr']),False)])
for sector,value in zip([43,44,45],data.values()): image[sector*2048:sector*2048+len(value)]=value
(root/'synthetic.iso').write_bytes(image)
(root/'expected.json').write_text(json.dumps({'iso':hashlib.sha256(image).hexdigest(),'files':{k:hashlib.sha256(v).hexdigest() for k,v in data.items()}}))
