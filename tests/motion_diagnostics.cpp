// Synthetic log-policy test: no game data, GPU, window or sleep.
#include "vk/motion_diagnostics.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <string>
static FILE *sink;
extern "C" int ngb_diag_writer_start(const char *path){sink=std::fopen(path,"w");return sink!=nullptr;}
extern "C" int ngb_diag_writer_active(void){return sink!=nullptr;}
extern "C" int ngb_diag_writer_enqueue(const char *text,size_t length){if(!sink)return 0;std::fwrite(text,1,length,sink);std::fflush(sink);return 1;}
int main(int argc,char** argv){assert(argc==2);setenv("NGB_MOTION_DIAGNOSTICS_FILE",argv[1],1);
 {NgbMotionDiagnostics d;
  auto frame=[&](int64_t t,unsigned n){d.Frame(t,n,1,true,false,100,10,2,1048576,2097152,1,2,3,4,5,6);};
  frame(1000000000,1);
  d.SceneInput(1048576,2);d.TargetSample(false);d.TargetSample(true);
  for(unsigned n=0;n<10;n++){d.Draw(n&1);d.Water(n,6,1,2,0,n&1,true,0x203);}
  int packets[2]{};d.MarkReplayPacket(&packets[0],0);d.MarkReplayPacket(&packets[1],9);
  d.ReplayPacket(&packets[0]);d.ReplayPacket(&packets[1]);d.ResetPackets();
  frame(1016666667,2);frame(1116666667,3);frame(2116666667,4);
 }
 if(sink){std::fclose(sink);sink=nullptr;}
 std::ifstream file(argv[1]);std::ostringstream out;out<<file.rdbuf();auto text=out.str();
 assert(text.find("requested=1 wanted=1")!=std::string::npos);
 assert(text.find("key_MiB=1.000 hits=2 target_readonly=1 target_replay=1")!=std::string::npos);
 assert(text.find("draws=10 generic=5")!=std::string::npos);
 assert(text.find("bump_draws=10 bump_replays=2 bump_overflow=2")!=std::string::npos);
 unsigned rows=0;for(size_t p=0;(p=text.find("[bump]",p))!=std::string::npos;p+=6)rows++;
 assert(rows==8);assert(text.size()<8192);
 std::puts("PASS: per-second summaries, bounded bump signatures, replay counts, and no GPU waits");
}
