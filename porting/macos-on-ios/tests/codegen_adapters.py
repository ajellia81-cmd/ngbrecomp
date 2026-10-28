"""Exercise the language adapter with the generator's real RECOMP_SPIN boundary."""
import importlib.util,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('wrap',root/'korean/wrap_game_codegen.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='ngb-generated-adapter-') as temp:
 folder=Path(temp)
 source='\n'.join('void sub_'+a+'(void) {}' for a in ['001E6AE0','0017E8A0','00232D40'])
 source+='\nvoid sub_00000001(void) {\n'+'\n'.join('x = MEM32(0xDB9890);' for _ in range(10))
 source+='\nRECOMP_SPIN(0x0037DC15u);\nRECOMP_SPIN(0x0037DC78u);\nRECOMP_SPIN(0x12345678u);\n}\n'
 (folder/'recomp_0000.c').write_text(source);(folder/'recomp_funcs.h').write_text('')
 module.wrap(folder);result=(folder/'recomp_0000.c').read_text()
 assert result.count('ngb_conversion_dsp_wait(')==2
 assert '\nRECOMP_SPIN(0x12345678u);' in result
 assert 'int ngb_conversion_dsp_wait(' in (folder/'recomp_funcs.h').read_text()
 print('Generated adapters: verified RECOMP_SPIN sites gated, other loops preserved')
