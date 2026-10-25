"""Render the actual Korean tab mask on macOS; no game assets are used."""
from pathlib import Path
import base64
import os
import struct
import subprocess
import tempfile
import zlib

ROOT=Path(__file__).resolve().parents[1]
def main():
    source=(ROOT/'ios/app.m').read_text()
    start=source.index('int ngb_ios_settings_tab_mask(')
    end=source.index('\nstatic int displayMilliHz',start)
    function=source[start:end]
    prefix='#import <Foundation/Foundation.h>\n#import <ImageIO/ImageIO.h>\n#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\n'
    suffix='\nint main(int argc,char **argv) { uint8_t mask[400*160]; if(argc!=2 || !ngb_ios_settings_tab_mask(mask,400,160)) return 1; FILE *f=fopen(argv[1],"wb"); if(!f) return 2; fwrite(mask,1,sizeof mask,f); return fclose(f); }\n'
    with tempfile.TemporaryDirectory() as directory:
        directory=Path(directory);code=directory/'preview.m';exe=directory/'preview';raw=directory/'mask.raw'
        code.write_text(prefix+function+suffix)
        subprocess.run(['xcrun','--sdk','macosx','clang','-fobjc-arc',str(code),'-framework','Foundation','-framework','ImageIO','-framework','CoreGraphics','-o',str(exe)],check=True)
        subprocess.run([str(exe),str(raw)],check=True,env={**os.environ,"NGB_SETTINGS_ART":str(ROOT/"ios/assets/settings-ko.png")})
        mask=raw.read_bytes()
    occupied=[(x,y) for y in range(160) for x in range(400) if mask[y*400+x]>16]
    assert occupied,'Empty Korean text mask'
    x0=min(x for x,y in occupied);x1=max(x for x,y in occupied)
    y0=min(y for x,y in occupied);y1=max(y for x,y in occupied)
    # Preview the plate-sized black lettering on white, using the same fitting
    # budget as the Korean tab. Human review checks glyph orientation and style.
    k=min(88/(x1-x0+1),32/(y1-y0+1),1)
    w,h=128,64;pixels=bytearray([255])*(w*h)
    ow=int((x1-x0+1)*k);oh=int((y1-y0+1)*k)
    ox=65-ow//2;oy=25-oh//2
    for y in range(oh):
        for x in range(ow):
            values=[mask[int(y0+(y+dy)/k)*400+int(x0+(x+dx)/k)] for dy in (.125,.375,.625,.875) for dx in (.125,.375,.625,.875)]
            pixels[(oy+y)*w+ox+x]=255-round(sum(values)/len(values))
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
    image=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pixels[y*w:(y+1)*w] for y in range(h))))+chunk(b'IEND',b'')
    print('Native Korean tab mask: bounding box',x1-x0+1,y1-y0+1,'plate size',ow,oh)
    print('NGB_TAB_PREVIEW_BASE64='+base64.b64encode(image).decode())
if __name__=='__main__': main()
