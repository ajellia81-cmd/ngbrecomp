// Diagnostic-only observer. Producers update fixed atomic counters; only the writer touches the file.
#include "performance_probe.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <time.h>
namespace {
enum Metric { GAME, SHOWN, FPS_US, GAME_GAP, SHOWN_GAP, MATCH, CUT, UNPAIRED, EMPTY,
 INTERP, INTERP_CPU, INTERP_CPU_MAX, INTERP_WALL, INTERP_WALL_MAX, REPLAYS, BROKEN, SINGLE,
 GPU_NS, GPU_N, GPU_MAX, WAIT_US, WAIT_N, WAIT_MAX, SYNC_COMPILE, SYNC_MAX, SHADERS, PIPES,
 DRAWS, DRAWN, MISSING, ORIGINAL, REPLAY, ASYNC_US, ASYNC_MAX, ASYNC_SHADER, ASYNC_PIPE, QUEUE_MAX, COUNT };
struct State { std::atomic<uint64_t> m[COUNT]{};std::atomic<int> mode{0},hz{0},gpu_support{-1};
 std::atomic<bool> running{false};std::mutex mu;std::condition_variable cv;std::thread writer;uint64_t writer_cpu_us=0; };
State& state(){static State *s=new State;return *s;}
void add(Metric m,uint64_t v){state().m[m].fetch_add(v,std::memory_order_relaxed);}
void high(Metric m,uint64_t v){auto& a=state().m[m];auto old=a.load(std::memory_order_relaxed);while(old<v&&!a.compare_exchange_weak(old,v,std::memory_order_relaxed)){} }
uint64_t clock_us(clockid_t c){timespec ts{};return clock_gettime(c,&ts)?0:uint64_t(ts.tv_sec)*1000000+uint64_t(ts.tv_nsec)/1000;}
void write_sample(FILE *f,uint64_t begin,uint64_t& last,uint64_t& cpu){
 uint64_t a[COUNT];for(int i=0;i<COUNT;i++)a[i]=state().m[i].exchange(0,std::memory_order_relaxed);
 auto now=clock_us(CLOCK_MONOTONIC),p=clock_us(CLOCK_PROCESS_CPUTIME_ID),dt=now-last;
 auto avg=[](uint64_t sum,uint64_t n,double divisor){return n?double(sum)/n/divisor:0.;};
 int thermal=-1,lowPower=-1,foreground=-1;ngb_perf_context(&thermal,&lowPower,&foreground);
 const double fps_scale=a[FPS_US]?1e6/a[FPS_US]:0.;
 std::fprintf(f,"{\"t_s\":%.3f,\"window_ms\":%.1f,\"gap\":%d,\"mode\":%d,\"hz\":%d,"
 "\"thermal\":%d,\"low_power\":%d,\"foreground\":%d,\"writer_cpu_previous_ms\":%.3f,\"game_fps\":%.2f,\"shown_fps\":%.2f,\"game_frames\":%llu,\"shown_frames\":%llu,\"fps_window_ms\":%.1f,\"game_gap_ms\":%.2f,\"shown_gap_ms\":%.2f,"
 "\"cpu_ms_per_s\":%.1f,\"match_frames\":%llu,\"skip_camera\":%llu,\"skip_unpaired\":%llu,\"skip_empty\":%llu,\"skip_broken\":%llu,\"single_picture\":%llu,\"replays\":%llu,"
 "\"interp_cpu_avg_ms\":%.3f,\"interp_cpu_max_ms\":%.3f,\"interp_wall_avg_ms\":%.3f,\"interp_wall_max_ms\":%.3f,"
 "\"gpu_supported\":%d,\"gpu_samples\":%llu,\"gpu_span_total_ms\":%.3f,\"gpu_span_avg_ms\":%.3f,\"gpu_span_max_ms\":%.3f,"
 "\"gpu_waits\":%llu,\"gpu_wait_total_ms\":%.3f,\"gpu_wait_max_ms\":%.3f,\"sync_compile_total_ms\":%.3f,\"sync_compile_max_ms\":%.3f,\"shaders\":%llu,\"pipelines\":%llu,"
 "\"draws\":%llu,\"drawn\":%llu,\"missing_pipeline_draws\":%llu,\"backend_original_frames\":%llu,\"backend_replay_frames\":%llu,"
 "\"async_compile_total_ms\":%.3f,\"async_compile_max_ms\":%.3f,\"async_shaders\":%llu,\"async_pipelines\":%llu,\"compile_queue_max\":%llu}\n",
 (now-begin)/1e6,dt/1000.,dt>3000000,int(state().mode.load()),int(state().hz.load()),thermal,lowPower,foreground,state().writer_cpu_us/1000.,
 a[GAME]*fps_scale,a[SHOWN]*fps_scale,(unsigned long long)a[GAME],(unsigned long long)a[SHOWN],a[FPS_US]/1000.,a[GAME_GAP]/1000.,a[SHOWN_GAP]/1000.,
 dt?double(p-cpu)*1000/dt:0.,(unsigned long long)a[MATCH],(unsigned long long)a[CUT],(unsigned long long)a[UNPAIRED],(unsigned long long)a[EMPTY],(unsigned long long)a[BROKEN],(unsigned long long)a[SINGLE],(unsigned long long)a[REPLAYS],
 avg(a[INTERP_CPU],a[INTERP],1000),a[INTERP_CPU_MAX]/1000.,avg(a[INTERP_WALL],a[INTERP],1000),a[INTERP_WALL_MAX]/1000.,
 int(state().gpu_support.load()),(unsigned long long)a[GPU_N],a[GPU_NS]/1e6,avg(a[GPU_NS],a[GPU_N],1e6),a[GPU_MAX]/1e6,
 (unsigned long long)a[WAIT_N],a[WAIT_US]/1000.,a[WAIT_MAX]/1000.,a[SYNC_COMPILE]/1000.,a[SYNC_MAX]/1000.,(unsigned long long)a[SHADERS],(unsigned long long)a[PIPES],
 (unsigned long long)a[DRAWS],(unsigned long long)a[DRAWN],(unsigned long long)a[MISSING],(unsigned long long)a[ORIGINAL],(unsigned long long)a[REPLAY],
 a[ASYNC_US]/1000.,a[ASYNC_MAX]/1000.,(unsigned long long)a[ASYNC_SHADER],(unsigned long long)a[ASYNC_PIPE],(unsigned long long)a[QUEUE_MAX]);
 std::fflush(f);last=now;cpu=p;
}
}
extern "C" uint64_t ngb_perf_cpu_us(){return clock_us(CLOCK_THREAD_CPUTIME_ID);}
extern "C" void ngb_perf_start(const char *path){
 if(!path||state().running.exchange(true))return;
 FILE *f=std::fopen(path,"a");if(!f){state().running=false;std::fprintf(stderr,"Cannot open fps-diagnostics.log\n");return;}
 state().writer=std::thread([f]{
  std::setvbuf(f,nullptr,_IOFBF,65536);
  std::fprintf(f,"{\"session\":true,\"format\":1,\"interval_s\":1,\"threaded_file_io\":true,\"gpu_queries_wait\":false,\"note\":\"GPU spans are command-buffer timestamp ranges; skip reasons can overlap; game_fps counts original flips; async compile sums can overlap frame work\"}\n");
  std::fflush(f);auto begin=clock_us(CLOCK_MONOTONIC),last=begin,cpu=clock_us(CLOCK_PROCESS_CPUTIME_ID);
  for(;;){std::unique_lock<std::mutex> l(state().mu);state().cv.wait_for(l,std::chrono::seconds(1),[]{return !state().running.load();});l.unlock();auto wc=ngb_perf_cpu_us();write_sample(f,begin,last,cpu);state().writer_cpu_us=ngb_perf_cpu_us()-wc;if(!state().running.load())break;}
  std::fclose(f);
 });std::atexit(ngb_perf_stop);
}
extern "C" void ngb_perf_stop(){if(!state().running.exchange(false))return;state().cv.notify_one();if(state().writer.joinable())state().writer.join();}
extern "C" void ngb_perf_mode(int mode,int hz){state().mode.store(mode);state().hz.store(hz);}
extern "C" void ngb_perf_fps(uint64_t game,uint64_t shown,uint64_t us,uint64_t gg,uint64_t sg){add(GAME,game);add(SHOWN,shown);add(FPS_US,us);high(GAME_GAP,gg);high(SHOWN_GAP,sg);}
extern "C" void ngb_perf_match(int cut,uint64_t matched,uint64_t draws){add(MATCH,1);if(cut)add(CUT,1);if(!draws)add(EMPTY,1);else if(matched*2<draws)add(UNPAIRED,1);}
extern "C" void ngb_perf_interp(uint64_t cpu,uint64_t wall,uint64_t replays,int pictures,int complete){add(INTERP,1);add(INTERP_CPU,cpu);high(INTERP_CPU_MAX,cpu);add(INTERP_WALL,wall);high(INTERP_WALL_MAX,wall);add(REPLAYS,replays);if(!complete)add(BROKEN,1);if(pictures<=1)add(SINGLE,1);}
extern "C" void ngb_perf_gpu_support(int s){state().gpu_support.store(s);}
extern "C" void ngb_perf_gpu(uint64_t ns){add(GPU_NS,ns);add(GPU_N,1);high(GPU_MAX,ns);}
extern "C" void ngb_perf_wait(uint64_t us){add(WAIT_US,us);add(WAIT_N,1);high(WAIT_MAX,us);}
extern "C" void ngb_perf_backend(uint64_t us,uint64_t sh,uint64_t p,uint64_t draws,uint64_t drawn,uint64_t missing,int replay){add(SYNC_COMPILE,us);high(SYNC_MAX,us);add(SHADERS,sh);add(PIPES,p);add(DRAWS,draws);add(DRAWN,drawn);add(MISSING,missing);add(replay?REPLAY:ORIGINAL,1);}
extern "C" void ngb_perf_async(uint64_t us,int p){add(ASYNC_US,us);high(ASYNC_MAX,us);add(p?ASYNC_PIPE:ASYNC_SHADER,1);}
extern "C" void ngb_perf_queue(uint64_t n){high(QUEUE_MAX,n);}
