import tempfile
import unittest
from pathlib import Path
from wrap_game_codegen import wrap

class HooksTest(unittest.TestCase):
    def test_preserves_calls_writes_and_unrelated_reads(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            source='void sub_001E6AE0(void) {}\nvoid sub_0017E8A0(void) {}\n'
            source+='void caller(void) { sub_001E6AE0(); }\n'
            source+='void f(void) {\n'+'x=MEM32(0xDB9890);\n'*11
            source+='MEM32(0xDB9890)=1;\nx=MEM32(0xDB9894);\n}\n'
            source+='void movie_tick(void) { RECOMP_ENTRY(0x0019A7D0u); }\n'
            source+='void movie_cue(void) { loc_0019A9CD: ; }\n'
            source+='void movie_enqueue(void) { RECOMP_ENTRY(0x00232D40u); }\n'
            source+='void movie_userdata(void) { loc_0031E1C5: ; }\n'
            (root/'recomp_0000.c').write_text(source)
            (root/'recomp_funcs.h').write_text('')
            wrap(root)
            result=(root/'recomp_0000.c').read_text()
            self.assertIn('void ngb_base_001E6AE0(void)',result)
            self.assertIn('sub_001E6AE0();',result)
            self.assertEqual(result.count('ngb_ios_game_text_language(MEM32(0xDB9890))'),11)
            self.assertIn('MEM32(0xDB9890)=1;',result)
            self.assertIn('MEM32(0xDB9894)',result)
            self.assertEqual(result.count('ngb_ios_movie_probe('),4)
if __name__=='__main__':unittest.main()
