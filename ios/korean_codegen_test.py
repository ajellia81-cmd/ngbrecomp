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

if __name__=='__main__':unittest.main()
