#pragma once
// Diagnostic-only, render-thread summaries. No captures, GPU queries or waits.
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstdint>
#include <cstdarg>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
class NgbMotionDiagnostics {
public:
 static int64_t Clock(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 enum Phase { EndpointDraw, ReplayDraw, MidpointGraph, PhaseCount };
 struct Timer {NgbMotionDiagnostics& d;Phase p;int64_t start;Timer(NgbMotionDiagnostics& d,Phase p):d(d),p(p),start(d.Enabled()?Clock():0){}~Timer(){if(start)d.cpu_[p]+=double(Clock()-start)/1e6;}};
 bool Enabled(){
  if(!opened_){opened_=true;const char* path=std::getenv("NGB_MOTION_DIAGNOSTICS_FILE");
   if(path&&*path){active_=true;try{writer_=std::thread(&NgbMotionDiagnostics::WriteLoop,this,std::string(path));}catch(...){active_=false;}}
  }
  return active_.load(std::memory_order_relaxed);
 }
 ~NgbMotionDiagnostics(){stop_=true;wake_.notify_one();if(writer_.joinable())writer_.join();}
 void Draw(bool generic){if(Enabled()){draws_++;generic_+=generic;}}
 void Water(uint64_t key,uint32_t modes,unsigned signed_mask,unsigned target_mask,unsigned shadow_mask,bool generic,bool blend,unsigned depth){
  if(!Enabled())return;
  water_++;
  for(auto& w:waters_)if(w.count&&w.key==key){w.count++;w.generic+=generic;return;}
  for(auto& w:waters_)if(!w.count){w={key,modes,signed_mask,target_mask,shadow_mask,1,unsigned(generic),unsigned(blend),depth};return;}
  overflow_++;
 }
 bool ReportDue(int64_t now){return Enabled()&&first_&&now-window_>=1000000000LL;}
 void ReplayWater(uint64_t key){if(!Enabled())return;water_replay_++;for(auto& w:waters_)if(w.count&&w.key==key){w.replays++;return;}}
 void Frame(int64_t now,unsigned frame,int requested,bool wanted,bool reset,unsigned scene,unsigned changed,unsigned equal,uint64_t packet,uint64_t snapshots,double extract_ms,double interpolate_ms,double project_ms,uint64_t mids,uint64_t ends,uint64_t superseded,double display_hz=0,unsigned width=0,unsigned height=0,unsigned msaa=0,bool capture_valid=true){
  if(!Enabled())return;
  if(!first_){first_=last_=window_=now;return;}
  double dt=double(now-last_)/1e6;last_=now;
  frames_++;peak_=std::max(peak_,dt);for(unsigned i=0;i<4;i++)slow_[i]+=dt>limits_[i];
  resets_+=reset;unwanted_+=!wanted;invalid_+=!capture_valid;scene_+=scene;changed_+=changed;equal_+=equal;packet_=std::max(packet_,packet);snapshots_=std::max(snapshots_,snapshots);
  extract_+=extract_ms;interpolate_+=interpolate_ms;project_+=project_ms;
  if(now-window_<1000000000LL)return;
  double seconds=double(now-window_)/1e9;
  std::string report;
  Append(report,"[frame] t=%.3f unix=%lld frame=%u game_hz=%.2f display_hz=%.1f target=%ux%u msaa=%u peak_ms=%.2f over20=%u over33=%u over50=%u over100=%u requested=%d wanted=%d resets=%u ineligible=%u invalid_capture=%u draws=%u generic=%u scene=%u changed=%u equal=%u packet_peak_MiB=%.2f snapshots_peak_MiB=%.2f endpoint_cpu_ms=%.2f replay_cpu_ms=%.2f midpoint_inclusive_cpu_ms=%.2f extract_ms=%.2f interpolate_ms=%.2f project_ms=%.2f midpoint_total=%llu endpoint_total=%llu superseded_total=%llu bump_draws=%u bump_replays=%u bump_overflow=%u log_dropped=%u\n",double(now-first_)/1e9,(long long)std::time(nullptr),frame,frames_/seconds,display_hz,width,height,msaa,peak_,slow_[0],slow_[1],slow_[2],slow_[3],requested,int(wanted),resets_,unwanted_,invalid_,draws_,generic_,scene_,changed_,equal_,packet_/1048576.,snapshots_/1048576.,cpu_[0],cpu_[1],cpu_[2],extract_,interpolate_,project_,(unsigned long long)mids,(unsigned long long)ends,(unsigned long long)superseded,water_,water_replay_,overflow_,dropped_);
  for(const auto& w:waters_)if(w.count)Append(report,"[bump] frame=%u shader=%016llx modes=%08x signed=%x target=%x shadow=%x blend=%u depth=%x draws=%u generic=%u replays=%u\n",frame,(unsigned long long)w.key,w.modes,w.signed_mask,w.target_mask,w.shadow_mask,w.blend,w.depth,w.count,w.generic,w.replays);
  frames_=draws_=generic_=water_=water_replay_=overflow_=resets_=unwanted_=invalid_=scene_=changed_=equal_=0;peak_=extract_=interpolate_=project_=0;packet_=snapshots_=0;cpu_={};slow_={};waters_={};window_=now;
  Enqueue(std::move(report));
 }
private:
 struct WaterRow{uint64_t key=0;uint32_t modes=0;unsigned signed_mask=0,target_mask=0,shadow_mask=0,count=0,generic=0,blend=0,depth=0,replays=0;};
 static void Append(std::string& text,const char* format,...){
  char row[4096];va_list args;va_start(args,format);int n=std::vsnprintf(row,sizeof row,format,args);va_end(args);
  if(n>0)text.append(row,std::min<size_t>(size_t(n),sizeof row-1));
 }
 // Single render-thread producer and disk-thread consumer. A full queue drops
 // diagnostics rather than blocking a game frame. No file I/O on the renderer.
 void Enqueue(std::string text){
  unsigned w=write_.load(std::memory_order_relaxed),r=read_.load(std::memory_order_acquire);
  if(w-r>=queue_.size()){dropped_++;return;}
  queue_[w%queue_.size()]=std::move(text);write_.store(w+1,std::memory_order_release);wake_.notify_one();
 }
 void WriteLoop(std::string path){
  FILE* file=std::fopen(path.c_str(),"w");if(!file){active_=false;return;}
  std::fprintf(file,"NGB motion diagnostics v1 unix=%lld; CPU timing only; midpoint CPU includes replay; water labels are bump candidates, not confirmed materials; asynchronous bounded writer\n",(long long)std::time(nullptr));
  for(;;){
   unsigned r=read_.load(std::memory_order_relaxed),w=write_.load(std::memory_order_acquire);
   if(r!=w){std::string text=std::move(queue_[r%queue_.size()]);read_.store(r+1,std::memory_order_release);
    std::fwrite(text.data(),1,text.size(),file);std::fflush(file);
    if(std::ftell(file)>=8*1024*1024){std::fputs("[limit] 8 MiB log limit reached; recording stopped\n",file);break;}
   }else if(stop_.load())break;
   else {std::unique_lock<std::mutex> lock(wait_mutex_);wake_.wait_for(lock,std::chrono::milliseconds(100),[&]{return stop_.load()||read_.load()!=write_.load();});}
  }
  std::fclose(file);active_=false;
 }
 std::array<std::string,4> queue_{};std::atomic<unsigned> read_{0},write_{0};
 std::atomic<bool> active_{false},stop_{false};std::thread writer_;std::mutex wait_mutex_;std::condition_variable wake_;unsigned dropped_=0;
 bool opened_=false;int64_t first_=0,last_=0,window_=0;
 unsigned frames_=0,draws_=0,generic_=0,water_=0,water_replay_=0,overflow_=0,resets_=0,unwanted_=0,invalid_=0,scene_=0,changed_=0,equal_=0;
 double peak_=0,extract_=0,interpolate_=0,project_=0;uint64_t packet_=0,snapshots_=0;
 std::array<double,PhaseCount> cpu_{};std::array<unsigned,4> slow_{};std::array<WaterRow,8> waters_{};
 static constexpr double limits_[4]={20,33.333,50,100};
};
