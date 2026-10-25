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

if __name__=='__main__':unittest.main()
