"""Run real concurrent producers and the asynchronous writer; verify measured aggregates."""
from pathlib import Path
import json,subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='ngb-perf-observer-') as td:
 work=Path(td);exe=work/'probe';log=work/'probe.log'
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-pthread','-Wall','-Wextra','-Werror','-I'+str(root),str(root/'performance_probe.cpp'),str(root/'tests/performance_probe.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(log)],check=True)
 lines=[json.loads(x) for x in log.read_text().splitlines()];assert lines[0]['session'] and lines[0]['threaded_file_io']
 rows=lines[1:];assert len(rows)>=2
 total=lambda k:sum(r[k] for r in rows)
 for k,v in dict(game_frames=60,shown_frames=120,match_frames=3,skip_camera=1,skip_unpaired=1,skip_empty=1,skip_broken=1,single_picture=1,replays=1,gpu_samples=40000,gpu_waits=1,shaders=1,pipelines=2,draws=200,drawn=195,missing_pipeline_draws=5,backend_original_frames=1,backend_replay_frames=1,async_shaders=1,async_pipelines=1).items():assert total(k)==v,(k,total(k),v)
 assert total('gpu_span_total_ms')==40000 and total('gpu_wait_total_ms')==5
 assert total('sync_compile_total_ms')==12 and total('async_compile_total_ms')==50
 r=next(r for r in rows if r['game_frames']);assert r['game_fps']==60 and r['shown_fps']==120
 assert max(r['interp_cpu_max_ms'] for r in rows)==3 and max(r['interp_wall_max_ms'] for r in rows)==16.667
 assert all(r['gpu_supported']==1 and r['thermal']==0 and r['foreground']==1 for r in rows)
 assert max(r['compile_queue_max'] for r in rows)==8
 # Apply/reverse the exact observation patch in a scratch tree, preserving all original frame decisions.
 repo=root.parents[1];paths=['src/d3d/gpu/threaded.cpp','src/d3d/vk/backend.cpp','xboxrecomp/src/nv2a_vk/host/present_sdl.c']
 for name in paths:
  dest=work/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((repo/name).read_bytes())
 patch=(root/'performance-observers.patch').read_bytes()
 subprocess.run(['git','apply','--unidiff-zero','-'],cwd=work,input=patch,check=True)
 threaded=(work/paths[0]).read_text();backend=(work/paths[1]).read_text()
 assert 'if (Match() && k > 1 && whole)' in threaded and 'bool ok = !cut && matched * 2 >= cur_draws_.size() && !cur_draws_.empty();' in threaded
 ts=backend.split('void TsRead(int slot)',1)[1].split('std::vector<VisQuery>',1)[0]
 assert 'VK_QUERY_RESULT_64_BIT) == VK_SUCCESS' in ts and 'VK_QUERY_RESULT_WAIT_BIT' not in ts
 assert 'ngb_perf_async(' in backend and 'ngb_perf_fps(' in (work/paths[2]).read_text()
 subprocess.run(['git','apply','--unidiff-zero','--reverse','-'],cwd=work,input=patch,check=True)
 for name in paths:assert (work/name).read_bytes()==(repo/name).read_bytes(),name
print('PASS: concurrent samples, exact aggregates, game versus shown FPS, skip reasons, bounded periodic writer, no added GPU waits and reversible observers')
