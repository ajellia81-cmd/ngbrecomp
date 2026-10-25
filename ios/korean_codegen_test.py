"""Synthetic fixtures only: verify the pinned font trampoline adaptation."""
import unittest
from merge_korean_codegen import repair_font_trampolines

def fixture():
    result={}
    for helper,target,frame in [('00DCA400','0021A23A',False),('00DCA600','0021A4B6',True)]:
        result['sub_'+helper]=('void sub_'+helper+'(void) {\n'+
            ('uint32_t ebp = g_seh_ebp;\n' if frame else '')+
            'PUSH32(esp, 0x'+target.lstrip('0')+');\n    esp += 4; return; /* ret */\n}')
        branch='g_seh_ebp = ebp; sub_'+helper+'(); return; /* tail jmp 0x'+helper+' */\n'
        result['caller_'+helper]=branch*(2 if not frame else 1)+'loc_'+target+': ;\n'
    return result

class Trampolines(unittest.TestCase):
    def test_resume_all_caller_paths_and_publish_selected_font_slot(self):
        bodies=fixture();repair_font_trampolines(bodies)
        for helper,target in [('00DCA400','0021A23A'),('00DCA600','0021A4B6')]:
            caller=bodies['caller_'+helper]
            self.assertNotIn('return;',caller)
            self.assertEqual(caller.count('goto loc_'+target),2 if helper=='00DCA400' else 1)
            self.assertNotIn('PUSH32',bodies['sub_'+helper])
        self.assertIn('g_seh_ebp = ebp; return;',bodies['sub_00DCA600'])
        self.assertIn('ebp = g_seh_ebp; goto',bodies['caller_00DCA600'])
    def test_reject_changed_patch_or_missing_branch(self):
        for key in ['sub_00DCA400','caller_00DCA600']:
            bodies=fixture();bodies[key]='changed'
            with self.assertRaises(ValueError):repair_font_trampolines(bodies)


# A real direct C call bypasses recomp_lookup_manual. Exercise both boot modes.
from pathlib import Path
import tempfile
import subprocess
from merge_korean_codegen import route_direct_entries

class DirectEntries(unittest.TestCase):
    def test_direct_and_indirect_calls_select_the_same_language(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'recomp_0000.c'
            source.write_text('int result;\nvoid sub_00012345(void);\nvoid sub_00012345(void) { result=10; }\n'
                'void direct_caller(void) { sub_00012345(); }\n'
                'void sub_001E6AE0(void) {}\n')
            routed=route_direct_entries(root,{'sub_00012345':'','sub_001E6AE0':''})
            self.assertEqual(routed,{'sub_00012345'})
            self.assertIn('void sub_001E6AE0(void) {}',source.read_text())
            source.write_text(source.read_text()+'''
static int korean;
int ngb_korean_active(void) { return korean; }
void kr_sub_00012345(void) { result=20; }
int main(void) {
    void (*indirect)(void)=sub_00012345;
    direct_caller(); if(result!=10) return 1;
    korean=1;
    direct_caller(); if(result!=20) return 2;
    indirect(); if(result!=20) return 3;
    korean=0;
    direct_caller(); if(result!=10) return 4;
    return 0;
}
''')
            executable=root/'test'
            subprocess.run(['cc','-DNGB_IOS_KOREAN=1',str(source),'-o',str(executable)],check=True,capture_output=True)
            subprocess.run([str(executable)],check=True)
            source.write_text(source.read_text().replace('result!=20','result!=10'))
            subprocess.run(['cc',str(source),'-o',str(executable)],check=True,capture_output=True)
            subprocess.run([str(executable)],check=True)


class InstructionRanges(unittest.TestCase):
    def test_identical_machine_code_is_retained_and_shortened_detour_checks_full_span(self):
        import struct
        from merge_korean_codegen import changed_instructions,select_changed_functions
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);image=bytearray(0x280)
            struct.pack_into('<I',image,0x104,0x10000)
            struct.pack_into('<II',image,0x11C,1,0x10180)
            struct.pack_into('<9I20s',image,0x180,0,0x1000,128,0x200,128,0,0,0,0,bytes(20))
            original=root/'original.bin';patched=root/'patched.bin'
            original.write_bytes(image);patched.write_bytes(image)
            old={'f':(0x1000,0x1040)};new={'f':(0x1000,0x1010)}
            self.assertFalse(changed_instructions('f',old,new,original,patched))
            self.assertEqual(select_changed_functions({'f':'safe call'},{'f':'direct call'},old,new,original,patched),{})
            image[0x230]=1;patched.write_bytes(image)
            self.assertTrue(changed_instructions('f',old,new,original,patched))
            self.assertEqual(set(select_changed_functions({'f':'safe call'},{'f':'direct call'},old,new,original,patched)),{'f'})


from merge_korean_codegen import repair_korean_spacing

class KoreanSpaces(unittest.TestCase):
    def test_measurement_and_both_draw_paths_match_compact_korean_spaces(self):
        def fixture():
            result={}
            for name,label,reg,index in [('measure','001E7462','ecx',0),('draw_a','001E8481','eax',1),('draw_b','001E8481','eax',2)]:
                parse='001E734F' if index==0 else '001E8147'
                result[name]=f'void {name}(void) {{ unsigned {reg};\nloc_{parse}: ;\nloc_{label}: ;\n    {reg} = ZX16(MEM16(esp + 0x88));\n result[{index}]={reg};\n}}\n'
            return result
        prefix=r'''#include <stdint.h>
#include "korean.h"
static uint32_t memory[128],esp,esi=0x100,edi=0x180,result[3];
#define MEM8(a) (*((uint8_t *)memory+(a)))
#define MEM32(a) memory[(a)/4]
#define MEM16(a) (*(uint16_t *)((uint8_t *)memory+(a)))
#define ZX16(a) ((uint16_t)(a))
'''
        main=r'''int main(void) {
 unsigned widths[]={0,1,7,16,17,32};
 for(unsigned font=0;font<3;font++) for(unsigned i=0;i<6;i++) {
  MEM32(esi+0x38)=font;MEM16(esp+0x88)=widths[i];
  measure();draw_a();draw_b();
  unsigned expected=font==1 ? (widths[i]+3)/4 : widths[i];
  if(result[0]!=expected || result[1]!=expected || result[2]!=expected) return 1;
 }
 MEM32(esi+0x38)=1; MEM16(esp+0x88)=16;
 edi=0x180; MEM8(edi)=0x81; MEM8(edi+1)=0x40;
 measure(); if(edi!=0x181 || result[0]!=4) return 2;
 edi=0x180; MEM32(esp+0x18)=edi; draw_a();
 if(MEM32(esp+0x18)!=0x181 || result[1]!=4) return 3;
 MEM32(esp+0x18)=edi; draw_b();
 if(MEM32(esp+0x18)!=0x181 || result[2]!=4) return 4;
 MEM32(esi+0x38)=0; edi=0x180; measure();
 if(edi!=0x180) return 5;
 return 0;
}'''
        with tempfile.TemporaryDirectory() as directory:
            directory=Path(directory)
            for name,adapt,success in [('fixed',True,True),('old',False,False)]:
                bodies=fixture()
                if adapt: repair_korean_spacing(bodies)
                source=directory/(name+'.c');exe=directory/name
                source.write_text(prefix+'\n'.join(bodies.values())+main)
                subprocess.run(['cc','-std=c11','-I'+str(Path(__file__).resolve().parents[1]/'src'),str(source),'-o',str(exe)],check=True,capture_output=True)
                completed=subprocess.run([str(exe)],capture_output=True)
                self.assertEqual(completed.returncode==0,success)
        broken=fixture();del broken['draw_b']
        with self.assertRaises(ValueError):repair_korean_spacing(broken)


from merge_korean_codegen import repair_korean_vertical_uv

class KoreanGlyphBottom(unittest.TestCase):
    def test_vertical_ink_row_retained_and_horizontal_gutter_unchanged(self):
        body=r'''void sample(void) {
 if(sprite==0x56d || sprite==0x587 || sprite==0x588) {
  xmm1.f[0] = xmm1.f[0] - MEMF(0xE0346C); /* subss */
  xmm0.f[0] = xmm0.f[0] - MEMF(0xE0346C); /* subss */
 }
}'''
        prefix=r'''#include <math.h>
static unsigned sprite;
static struct { float f[4]; } xmm0,xmm1;
#define MEMF(a) (1.0f/256)
// A synthetic font cell with its thin final stroke on row 15.
static float final_ink(float v) { float d=fabsf(v*256-.5f-15); return d<1 ? 1-d : 0; }
'''
        main=r'''int main(void) {
 unsigned ids[]={0x56d,0x587,0x588,0x123};
 for(unsigned i=0;i<4;i++) {
  sprite=ids[i];xmm0.f[0]=17.0f/256;xmm1.f[0]=16.5f/256;sample();
  if(i==3 && fabsf(xmm0.f[0]-17.0f/256)>1e-7f) return 1;
  float expected=(i<3 ? 15.5f : 16.5f)/256;
  if(fabsf(xmm1.f[0]-expected)>1e-7f) return 2;
  // The last safe console-pixel sample must retain the bottom stroke before
  // fractional quad snapping excludes the destination's boundary pixel.
  float v=1.5f/256+(xmm0.f[0]-1.5f/256)*(14.5f/16);
  if(final_ink(v)<.9f) return 3;
 }
 return 0;
}'''
        with tempfile.TemporaryDirectory() as directory:
            directory=Path(directory)
            for name,adapt,success in [('fixed',True,True),('old',False,False)]:
                bodies={'sub_00E03430':body}
                if adapt:repair_korean_vertical_uv(bodies)
                source=directory/(name+'.c');exe=directory/name
                source.write_text(prefix+bodies['sub_00E03430']+main)
                subprocess.run(['cc','-std=c11',str(source),'-lm','-o',str(exe)],check=True,capture_output=True)
                result=subprocess.run([str(exe)],capture_output=True)
                self.assertEqual(result.returncode==0,success)
        with self.assertRaises(ValueError):repair_korean_vertical_uv({'sub_00E03430':'changed'})


class KoreanSpriteCoverage(unittest.TestCase):
    def test_last_ink_row_fades_inside_quad_for_every_font_page(self):
        source=(Path(__file__).resolve().parents[1]/'src/pause_tab.c').read_text()
        start=source.index('static void edge_inset(');a=source.index('{',start);end=a+1;depth=1
        while depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        body=source[start:end]
        prefix=r'''
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
static uint8_t mem[0x8e0000];
#define MEM32(a) (*(uint32_t *)(mem+(a)))
#define MEMF(a) (*(float *)(mem+(a)))
static int scale,active;
int ngb_pc_setting(int option) { (void)option;return scale; }
int ngb_korean_active(void) { return active; }
uint32_t sprite_def(uint32_t id) { (void)id;return 0x100; }
'''
        main=r'''
// Last ink row centred on texel 15.5, next row transparent. A non-zero
// limiting alpha at the quad edge is an abrupt cut, rather than a full fade.
static float bottom_alpha(float lo,float hi) {
 float edge=(hi-(hi-lo)*.5f/15.5f)*256;
 float a=16.5f-edge;
 return fmaxf(0,fminf(1,a));
}
int main(void) {
 unsigned ids[]={0x56d,0x587,0x588,0x100};
 MEM32(0x110)=1;MEM32(0x8DBB30)=0x200;
 MEM32(0x20C)=(8u<<20)|(8u<<24);
 for(active=0;active<=1;active++) for(scale=1;scale<=8;scale++)
 for(unsigned i=0;i<4;i++) {
  MEM32(0x300)=ids[i];
  for(unsigned v=0;v<4;v++) {
   MEMF(0x400+v*28+20)=(v&1 ? 15.5f : .5f)/256;
   MEMF(0x400+v*28+24)=(v&2 ? 17.f : 1.5f)/256;
  }
  edge_inset(0x300,0x400);
  float lo=MEMF(0x418),hi=MEMF(0x450);
  if(active && i<3) {
   if(lo!=1.5f/256 || hi!=17.f/256) return 1;
   if(bottom_alpha(lo,hi)>.001f) return 2;
  } else if(scale>1 && !(lo>1.5f/256 && hi<17.f/256)) return 3;
  if(scale>1 && !(MEMF(0x414)>.5f/256)) return 4;
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for name,define,success in [('fixed','-DNGB_IOS_KOREAN',True),('old','-UNGB_IOS_KOREAN',False)]:
                p=root/(name+'.c');exe=root/name;p.write_text(prefix+body+main)
                subprocess.run(['cc','-std=c11',define,str(p),'-lm','-o',str(exe)],check=True,capture_output=True)
                r=subprocess.run([str(exe)])
                self.assertEqual(r.returncode==0,success)

if __name__=='__main__':unittest.main()
