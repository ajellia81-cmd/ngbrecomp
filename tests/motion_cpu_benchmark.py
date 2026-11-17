#!/usr/bin/env python3
"""Public synthetic Apple CPU comparison; no game, window, audio, or user data."""
import argparse,json,random,re,statistics,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
reference='08c8419338b0faa942eae0ded5836e55f5040e5b'
def function(source,name):
 start=source.index(name);brace=source.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
def run(args):return subprocess.check_output([str(x) for x in args],text=True,cwd=root)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--temp',type=Path,required=True);args=parser.parse_args();args.temp.mkdir(parents=True,exist_ok=False)
 old=run(['git','show',reference+':src/d3d/gpu/scene_motion.cpp']);new=(root/'src/d3d/gpu/scene_motion.cpp').read_text()
 old_matrix=function(old,'V transform_point(');new_matrix=function(new,'V transform_point(')
 old_execute=function(old,'bool execute(');new_execute=function(new,'bool execute(')
 a=old_execute.index('        for(int i=0;i<4;i++) switch(o.mac) {');b=old_execute.index('        switch(o.ilu)',a)
 c=new_execute.index('        switch(o.mac) {');d=new_execute.index('        switch(o.ilu)',c)
 dispatched=old_execute[:a]+new_execute[c:d]+old_execute[b:]
 variants={'reference':old,'full':new,'fold_scalar':new.replace(new_matrix,old_matrix),'simd':old.replace(old_matrix,new_matrix),'dispatch':old.replace(old_execute,dispatched)}
 binaries={}
 for name,source in variants.items():
  path=args.temp/(name+'.cpp');path.write_text(source);binary=args.temp/name
  command=['xcrun','--sdk','macosx','clang++','-std=c++20','-O2','-DNGB_SMOOTH_MOTION=1','-DNGB_IOS_PORT_RELEASE=1','-DNGB_IOS_MOTION_DIAGNOSTICS=1','-DNGB_IOS_PERFORMANCE_DIAGNOSTICS=1','-ffp-contract=off','-I'+str(root/'src/d3d'),root/'tests/motion_cpu_parity.cpp',root/'src/d3d/gpu/shaders.cpp',path,root/'third_party/vf3_scene/interpolate.cpp','-o',binary]
  subprocess.run([str(x) for x in command],cwd=root,check=True);binaries[name]=binary
 values={name:[] for name in variants};expected=None;randomizer=random.Random(46)
 for iteration in range(10):
  order=list(variants);randomizer.shuffle(order)
  for name in order:
   output=run([binaries[name]]);checksum=re.search(r'parity=(\w+)',output).group(1)
   if expected is None:expected=checksum
   if checksum!=expected:raise SystemExit('Output mismatch: '+name)
   add,resolve=map(float,re.search(r'add_ms=([\d.e+-]+) resolve_ms=([\d.e+-]+)',output).groups())
   if iteration:values[name].append((add,resolve))
 summary={name:{'add_median_ms':statistics.median(x[0] for x in data),'resolve_median_ms':statistics.median(x[1] for x in data),'total_median_ms':statistics.median(sum(x) for x in data),'samples':data} for name,data in values.items()}
 print('CPU_BENCHMARK '+json.dumps({'parity':expected,'rounds':9,'results':summary},sort_keys=True))
if __name__=='__main__':main()
