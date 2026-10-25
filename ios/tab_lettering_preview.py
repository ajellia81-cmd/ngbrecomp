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
    tab_source=(ROOT/'src/pause_tab.c').read_text()
    start=tab_source.index('static void tab_word(')
    end=tab_source.index('\nstatic inline uint32_t mix',start)
    tab=tab_source[start:end]
    prefix='#import <Foundation/Foundation.h>\n#import <ImageIO/ImageIO.h>\n#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\n#include <math.h>\n#define NGB_IOS_KOREAN 1\n#define draw_text(...) 0.f\nstatic int s_game_font;\nint ngb_korean_active(void) { return 1; }\n'
    suffix='\nint main(int argc,char **argv) { uint8_t mask[128*64]={0}; if(argc!=2) return 1; tab_word(mask,128,64,65,25,88,38); FILE *f=fopen(argv[1],"wb"); if(!f) return 2; fwrite(mask,1,sizeof mask,f); return fclose(f); }\n'
    with tempfile.TemporaryDirectory() as directory:
        directory=Path(directory);code=directory/'preview.m';exe=directory/'preview';raw=directory/'mask.raw'
        code.write_text(prefix+function+tab+suffix)
        subprocess.run(['xcrun','--sdk','macosx','clang','-fobjc-arc',str(code),'-framework','Foundation','-framework','ImageIO','-framework','CoreGraphics','-o',str(exe)],check=True)
        subprocess.run([str(exe),str(raw)],check=True,env={**os.environ,"NGB_SETTINGS_ART":str(ROOT/"ios/assets/settings-ko.png")})
        mask=raw.read_bytes()
    w,h=128,64
    occupied=[(x,y) for y in range(h) for x in range(w) if mask[y*w+x]>16]
    assert occupied,'Empty Korean text mask'
    x0=min(x for x,y in occupied);x1=max(x for x,y in occupied)
    y0=min(y for x,y in occupied);y1=max(y for x,y in occupied)
    pixels=bytes(255-v for v in mask)
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
    image=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pixels[y*w:(y+1)*w] for y in range(h))))+chunk(b'IEND',b'')
    print('Native Korean tab mask: bounding box',x1-x0+1,y1-y0+1,'plate bounds',x0,y0,x1,y1)
    print('NGB_TAB_PREVIEW_BASE64='+base64.b64encode(image).decode())
if __name__=='__main__': main()
