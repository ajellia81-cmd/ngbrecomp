// Synthetic timing correlation and bounded-output regression; no GPU or sleeps.
#include "vk/performance_diagnostics.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
static FILE* sink;
static unsigned batches;
extern "C" int ngb_diag_writer_start(const char* path){sink=std::fopen(path,"w");return sink!=nullptr;}
extern "C" int ngb_diag_writer_active(void){return sink!=nullptr;}
extern "C" int ngb_diag_writer_enqueue(const char* data,size_t size){assert(size<8192);batches++;std::fwrite(data,1,size,sink);std::fflush(sink);return 1;}
int main(int argc,char** argv){
 assert(argc==2);setenv("NGB_MOTION_DIAGNOSTICS_FILE",argv[1],1);
 {
  NgbPerformanceDiagnostics d;int64_t now=1000000000;d.Complete(now,1);
  {NgbPerformanceDiagnostics::WorkerTimer worker(d,false);}
  auto frame=[&](unsigned n,int64_t dt,int64_t present,int64_t wait){
   d.Draw(true);d.Draw(false);d.Projection(2,3);
   d.SceneInput(1048576,4,2097152,0);d.SceneWork(0,0,0,5,6,0);
   d.BeginPresent(now+dt-present,4000,1,2);
   d.Note(NgbPerformanceDiagnostics::GpuWait,wait);
   d.Note(NgbPerformanceDiagnostics::FrameEnd,wait+1000000);
   d.Note(NgbPerformanceDiagnostics::Resolve,2000000);
   d.Frame(now,n,1,true,false,100,42,0,1048576,2097152,0,0,0,3,4,5,120,1280,720,1,true);
   now+=dt;d.Complete(now,n);
  };
  frame(2,120000000,40000000,30000000);
  // Many hitches must not create one file write per hitch or unbounded rows.
  for(unsigned n=3;n<=38;n++)frame(n,30000000,5000000,0);
  assert(batches==1);
  // Old per-draw phases must perform no sampling or accumulation in this mode.
  {NgbPerformanceDiagnostics::Timer ignored(d,NgbPerformanceDiagnostics::EndpointDraw);assert(ignored.start==0);}
 }
 std::fclose(sink);sink=nullptr;
 std::ifstream f(argv[1]);std::ostringstream out;out<<f.rdbuf();const auto text=out.str();
 assert(text.find("[performance]")!=std::string::npos);
 assert(text.find("gpu_fence_wait_ms=30.00")!=std::string::npos);
 assert(text.find("frame=2 dt_ms=120.00 present_ms=40.00 outside_present_ms=80.00 sync_compile_ms=4.00")!=std::string::npos);
 assert(text.find("requested=1 wanted=1 valid=1 scene=100 changed=42")!=std::string::npos);
 assert(text.find("[scene-cpu]")!=std::string::npos);
 assert(text.find("draws=2 generic=1")!=std::string::npos);
 assert(text.find("kind=shader jobs=1")!=std::string::npos);
 assert(text.find("kind=pipeline")!=std::string::npos);
 unsigned rows=0;for(size_t i=0;(i=text.find("[slow-frame]",i))!=std::string::npos;i+=12)rows++;
 assert(rows==8);assert(text.find("dropped_slow_rows=0")==std::string::npos);
 assert(text.find("water-program")==std::string::npos && text.find("[bump]")==std::string::npos);
 puts("PASS: complete-frame timing includes final waits, correlated slow frames, bounded async summaries, no per-draw sampling or water data");
}
