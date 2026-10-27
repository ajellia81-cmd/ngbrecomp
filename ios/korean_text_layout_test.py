"""Native replay of text layout helpers, with synthetic GPU glyphs only."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from merge_korean_codegen import repair_korean_text_layout
ROOT=Path(__file__).resolve().parents[1]

class TextLayout(unittest.TestCase):
    def test_native_glyph_ink_spacing_and_paragraph_geometry(self):
        helper=(ROOT/'ios/korean_text_layout.c').read_text()
        helper=helper[helper.index('int ngb_korean_punctuation_at'):]
        prefix=r'''
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "korean.h"
static unsigned char ram[16*1024*1024];
#define AT(a) (ram+((uint32_t)(a)&0x7FFFFFFFu))
#define MEM8(a) (*(uint8_t *)AT(a))
#define MEM16(a) (*(uint16_t *)AT(a))
#define MEM32(a) (*(uint32_t *)AT(a))
#define MEMF(a) (*(float *)AT(a))
static unsigned address(unsigned x,unsigned y) {
 unsigned a=0;for(unsigned i=0;i<8;i++) a|=((x>>i)&1)<<(i*2)|((y>>i)&1)<<(i*2+1);return a;
}
'''
        main=r'''
int main(void) {
 unsigned state=0x100,ws=0x200,xs=0x400,def=0x3000,hdr=0x2000,text=0x1000;
 MEMF(0x43B494)=1;MEM32(0x8DBB28)=hdr;
 MEM32(hdr+4)=0x4000;MEM32(hdr+12)=(8<<20)|(8<<24)|(0x19<<8);
 MEMF(def)=0;MEMF(def+4)=0;MEMF(def+8)=16.0f/256;MEMF(def+12)=16.0f/256;
 MEM32(def+16)=0;MEM32(def+20)=256|(256<<16);
 MEM8(text)=0x81;MEM8(text+1)=0x44;
 for(unsigned y=13;y<15;y++)for(unsigned x=8;x<10;x++)MEM8(0x4000+address(x,y))=x==8?255:1;
 unsigned char before[65536];memcpy(before,AT(0x4000),sizeof before);
 for(unsigned kind_index=0;kind_index<4;kind_index++) {
  unsigned kind=(unsigned[]){0x19,11,6,4}[kind_index];MEM32(hdr+12)=(8<<20)|(8<<24)|(kind<<8);
  if(kind==11){MEM32(0x8DBB2C)=0x2800;MEM32(0x2804)=0x30000;MEM32(0x30004)=0xFFFFFFFF;}
  for(unsigned y=13;y<15;y++)for(unsigned x=8;x<10;x++) {
   unsigned off=address(x,y);
   if(kind==11)MEM8(0x4000+off)=1;
   if(kind==6)MEM32(0x4000+off*4)=x==8?0xFFFFFFFF:0x01FFFFFF;
   if(kind==4)MEM16(0x4000+off*2)=0xFFFF;
  }
  // Distinct resources ensure each format is decoded independently.
  if(kind_index) {MEM32(hdr+4)=0x4000+kind_index*0x100000;
   memcpy(AT(MEM32(hdr+4)),AT(0x4000),kind==6?262144:kind==4?131072:65536);}
  for(unsigned i=0;i<4;i++) {
   float scale=(float[]){.5f,1,1.25f,2}[i];
   unsigned original=(unsigned)nearbyintf(16*scale);
   int left=ngb_korean_punctuation_left(def,text,1,scale);
   unsigned advance=ngb_korean_punctuation_advance(original,def,text,1,scale);
   assert(left>=0 && advance<original);
   int pen=100-left;
   int next=pen+advance+left;
   assert(next==100+(int)advance); // Same advance as both measuring passes.
   assert(100-left+10*scale<=next+.01f); // All ink fits before the next glyph.
   assert(ngb_korean_punctuation_left(def,text,0,scale)==0);
   assert(ngb_korean_punctuation_advance(original,def,text,0,scale)==original);
  }
 }
 MEM32(hdr+12)=(8<<20)|(8<<24)|(0x19<<8);MEM32(hdr+4)=0x4000;
 memcpy(AT(0x4000),before,sizeof before);
 assert(ngb_korean_punctuation_left(def,text,1,.5f)==4);
 assert(ngb_korean_punctuation_advance(8,def,text,1,.5f)==2);
 assert(!memcmp(before,AT(0x4000),sizeof before)); // Sampling leaves all original pixels intact.
 // Period/comma ink can occupy different columns. Both receive exactly the
 // same leading/trailing side bearings; neither is centered in its cell.
 unsigned comma=def+64;
 memcpy(AT(comma),AT(def),24);MEMF(comma)=16.0f/256;MEMF(comma+8)=32.0f/256;
 for(unsigned y=13;y<15;y++)for(unsigned x=18;x<21;x++)MEM8(0x4000+address(x,y))=255;
 for(unsigned which=0;which<2;which++) {
  MEM8(text)=0x81;MEM8(text+1)=which?0x43:0x44;
  unsigned glyph=which?comma:def;
  int left=ngb_korean_punctuation_left(glyph,text,1,1);
  unsigned advance=ngb_korean_punctuation_advance(16,glyph,text,1,1);
  int ink_left=(which?2:8)-left,ink_right=(which?5:10)-left;
  assert(ink_left==0 && (int)advance-ink_right==2);
 }
 MEM8(text)='A';assert(!ngb_korean_punctuation_at(text));
 assert(ngb_korean_punctuation_advance(8,def,text,1,.5f)==8);
 for(unsigned t=0;t<8;t++){MEM8(text)=0x81;MEM8(text+1)=(unsigned[]){0x41,0x42,0x43,0x44,0x46,0x47,0x48,0x49}[t];assert(ngb_korean_punctuation_at(text));}
 MEM32(state+0x38)=1;MEM16(state+0x44)=70;MEM16(state+0x46)=60;
 unsigned widths[]={510,470,0,420};
 for(unsigned i=0;i<4;i++){MEM16(ws+i*2)=widths[i];MEM16(xs+i*2)=70;}
 ngb_korean_center_paragraph(state,ws,xs,4,0x205574);
 for(unsigned i=0;i<4;i++)assert(MEM16(xs+i*2)==105);
 assert(MEM16(xs)*2+510==720); // Whole block center, not each individual line.
 assert(MEM16(state+0x44)==70 && MEM16(state+0x46)==60);
 // A two-line wide prose block also qualifies; indentation is translated,
 // not replaced by per-line centering.
 MEM16(ws)=400;MEM16(ws+2)=360;MEM16(xs)=70;MEM16(xs+2)=90;
 ngb_korean_center_paragraph(state,ws,xs,2,0x123456);
 assert(MEM16(xs)==160 && MEM16(xs+2)==180);
 unsigned char unchanged[8];memcpy(unchanged,AT(xs),8);
 for(unsigned scenario=0;scenario<5;scenario++) {
  MEM32(state+0x38)=scenario==0?0:1;
  MEM8(state+0x4A)=scenario==1?1:0;
  MEM16(state+0x44)=scenario==2?380:70;
  MEM16(state+0x46)=scenario==3?420:60;
  unsigned count=scenario==4?1:4;
  ngb_korean_center_paragraph(state,ws,xs,count,0x123456);
  assert(!memcmp(unchanged,AT(xs),8));
 }
 // Narrow full-page loading prose and scrolling keep their common origin.
 MEM32(state+0x38)=1;MEM8(state+0x4A)=0;MEM16(state+0x44)=70;MEM16(state+0x46)=(uint16_t)-20;
 MEM16(ws)=240;MEM16(ws+2)=180;MEM16(xs)=70;MEM16(xs+2)=70;
 ngb_korean_center_paragraph(state,ws,xs,2,0x205574);
 assert(MEM16(xs)==240 && MEM16(xs+2)==240);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);source=p/'test.c';exe=p/'test'
            source.write_text(prefix+helper+main)
            subprocess.run([os.environ.get('CC','cc'),'-std=c11','-I'+str(ROOT/'src'),str(source),'-lm','-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_codegen_guards_require_all_measurement_and_draw_paths(self):
        labels=['001E8CC6','001E8B55','001E8BB8','001E734F','001E73A9','001E73EF',
                '001E8C00','001E8C41','001E818A','001E83F8','001E7527']
        body='void fixture(void) {\n'+''.join('loc_'+x+': ;\n'+('    eax = ZX16(MEM16(esp + 0x2C));\n' if x=='001E8CC6' else '') for x in labels)
        body+='loc_001E7462: ;\n    ecx = ngb_korean_space_advance(ecx,MEM32(esi + 0x38));\n'
        body+='loc_001E8481: ;\n    eax = ngb_korean_space_advance(eax,MEM32(esi + 0x38));\n}'
        duplicate='void duplicate(void) {\nloc_001E818A: ;\nloc_001E83F8: ;\nloc_001E8481: ;\n    eax = ngb_korean_space_advance(eax,MEM32(esi + 0x38));\n}'
        bodies={'a':body,'b':duplicate};repair_korean_text_layout(bodies)
        self.assertIn('MEM32(ebp+20)',bodies['a'])
        self.assertNotIn('esi++; goto loc_001E8CC6',bodies['a'])
        self.assertIn('ngb_korean_center_paragraph',bodies['a'])
        self.assertEqual(bodies['a'].count('unsigned ngb_punct_def=0;'),1)
        # The public measurer selects the original horizontal compression.
        # No compact spacing hook may enter it, even for punctuation or spaces.
        legacy='void legacy(void) {\n'+''.join('loc_'+x+': ;\n' for x in
            ['001E8CC6','001E8B55','001E8BB8','001E8C00','001E8C41'])+'}'
        bodies={'a':body,'b':duplicate,'legacy':legacy}
        repair_korean_text_layout(bodies)
        self.assertEqual(bodies['legacy'],legacy)

        self.assertIn('*MEMF(esp+0xAC)',bodies['b'])
        with self.assertRaises(ValueError):repair_korean_text_layout({'a':body})

if __name__=='__main__':unittest.main()
