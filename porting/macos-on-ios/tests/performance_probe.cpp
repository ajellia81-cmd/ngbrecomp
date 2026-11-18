#include "performance_probe.h"
#include <thread>
#include <chrono>
#include <cassert>
#include <cstdio>
extern "C" void ngb_perf_context(int *t,int *p,int *f){*t=0;*p=0;*f=1;}
int main(int argc,char **argv){
 assert(argc==2);ngb_perf_start(argv[1]);ngb_perf_mode(1,120);ngb_perf_gpu_support(1);
 ngb_perf_fps(30,60,500000,16667,8333);ngb_perf_fps(30,60,500000,16667,8333);
 ngb_perf_match(1,100,100);ngb_perf_match(0,49,100);ngb_perf_match(0,0,0);
 ngb_perf_interp(1000,8333,1,2,1);ngb_perf_interp(2000,16667,0,2,0);ngb_perf_interp(3000,16667,0,1,1);
 ngb_perf_wait(5000);ngb_perf_backend(12000,1,2,100,95,5,0);ngb_perf_backend(0,0,0,100,100,0,1);
 ngb_perf_async(20000,0);ngb_perf_async(30000,1);ngb_perf_queue(8);
 auto start=std::chrono::steady_clock::now();
 std::thread producers[4];for(auto& t:producers)t=std::thread([]{for(int i=0;i<10000;i++)ngb_perf_gpu(1000000);});
 for(auto& t:producers)t.join();
 auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 std::printf("Observer stress: 40000 concurrent GPU samples in %lld us (no per-sample file writes)\n",(long long)us);
 std::this_thread::sleep_for(std::chrono::milliseconds(1150));ngb_perf_stop();
}
