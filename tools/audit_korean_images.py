#!/usr/bin/env python3
"""Audit private Korean sprite images, including non-menu screens and maps.

Requires numpy; optional contact sheets require Pillow. Run only on private
verified inputs. Reports and previews are local investigation output, not app
resources, and must not be committed with game data.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'ios'))
from korean_sprite_layout import SpritePackage


CENTERED = {
    'demo.afs/main_menu.spr': set(range(3)),
    'spr.afs/arcade.spr': {2,3,4},
    'spr.afs/btl_menu.spr': {0},
    'spr.afs/cos_menu.spr': {0},
    'spr.afs/clear.spr': {*range(28,33),*range(37,43),44,45,46},
    'spr.afs/cpstart.spr': set(range(6,74)),
    'spr.afs/gameover.spr': {0,2,3,4,5,6},
    'spr.afs/gameovr2.spr': {0,2,3,4,5,6},
    'spr.afs/mis_clear.spr': {0,1,2,3,4,15,16},
    'spr.afs/mis_menu.spr': {0},
    'spr.afs/saveload.spr': {1,2,36},
}
LEFT_LABELS = {
    'spr.afs/shop.spr': {6,7,8,22,37,38,39,40,51,58},
    'spr.afs/saveload.spr': {25,26,27,28,29,30,33,40,41,42,43},
    'spr.afs/clear.spr': {11,12,13,14,15},
}


def role(member,index,count):
    if count==1:
        return 'font-bank' if Path(member).stem in ('fjp_bk02','fjp_st15','fjp_st16') else 'map-with-embedded-labels'
    if member=='spr.afs/option.spr' and 13<=index<=18:return 'center-in-button'
    if index in CENTERED.get(member,()):return 'center-in-quad'
    if index in LEFT_LABELS.get(member,()):return 'left-label'
    return 'artwork-composite-or-original-language'


def rgba(package,texture):
    w,h,kind,offset,palette=package.textures[texture]
    yy,xx=np.indices((h,w),dtype=np.uint32);addresses=np.zeros((h,w),dtype=np.uint32)
    bit,shift=1,0
    while bit<max(w,h):
        if bit<w:addresses|=((xx&bit)!=0).astype(np.uint32)<<shift;shift+=1
        if bit<h:addresses|=((yy&bit)!=0).astype(np.uint32)<<shift;shift+=1
        bit<<=1
    if kind==6:colors=np.frombuffer(package.data,dtype='<u4',count=w*h,offset=offset)[addresses]
    else:
        values=np.frombuffer(package.data,dtype='u1',count=w*h,offset=offset)[addresses]
        colors=np.asarray(palette,dtype=np.uint32)[values] if kind==11 else values.astype(np.uint32)<<24|0xFFFFFF
    return np.stack([(colors>>s)&255 for s in (16,8,0,24)],axis=2).astype('uint8')


def audit(root,previews=None):
    if previews:
        from PIL import Image,ImageDraw
        previews.mkdir(parents=True,exist_ok=True)
    rows=[]
    for path in sorted(root.rglob('*.spr')):
        member=path.relative_to(root).as_posix();package=SpritePackage(path.read_bytes())
        decoded={};thumbs=[]
        for index in range(package.count):
            texture,box,size=package.definition(index,allow_overflow=True)
            if texture not in decoded:decoded[texture]=rgba(package,texture)
            x0,y0,x1,y1=box;pixels=decoded[texture]
            crop=pixels[max(0,y0):max(0,y1),max(0,x0):max(0,x1)]
            # Include faint glow/shadow pixels in bounds, so they are not
            # mistaken for clipping or trimmed away by an alignment change.
            ys,xs=np.nonzero((crop[:,:,3]>0)&(crop[:,:,:3].max(axis=2)>120))
            bounds=[int(xs.min()),int(ys.min()),int(xs.max()+1),int(ys.max()+1)] if len(xs) else None
            alignment=role(member,index,package.count);width=x1-x0
            target=101.5 if alignment=='center-in-button' else width/2
            error=(bounds[0]+bounds[2])/2-target if bounds and alignment.startswith('center-') else None
            rows.append(dict(member=member,sprite=index,texture=texture,box=box,
                             alignment=alignment,ink=bounds,center_error=error,
                             window_overflow=(x0<0 or y0<0 or x1>pixels.shape[1] or y1>pixels.shape[0])))
            if previews:
                im=Image.fromarray(crop);bg=Image.new('RGB',im.size,(45,45,45));bg.paste(im,mask=im.getchannel('A'))
                bg.thumbnail((350,160));thumbs.append((index,bg))
        if previews:
            # Pages never resize the complete sheet down into illegible text.
            for first in range(0,len(thumbs),18):
                group=thumbs[first:first+18];sheet=Image.new('RGB',(1080,((len(group)+2)//3)*190),(24,24,24));draw=ImageDraw.Draw(sheet)
                for j,(index,im) in enumerate(group):
                    x=j%3*360;y=j//3*190;draw.text((x+4,y+3),f'{member} #{index}',fill='white');sheet.paste(im,(x+4,y+22))
                sheet.save(previews/(member.replace('/','_')+f'-{first//18}.png'))
    return dict(files=len(set(r['member'] for r in rows)),sprites=len(rows),
                roles=dict(Counter(r['alignment'] for r in rows)),
                center_outliers=[r for r in rows if r['center_error'] is not None and abs(r['center_error'])>2],sprites_detail=rows)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--members',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--previews',type=Path)
    args=parser.parse_args();report=audit(args.members,args.previews)
    args.report.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='sprites_detail'}))


if __name__=='__main__':main()
