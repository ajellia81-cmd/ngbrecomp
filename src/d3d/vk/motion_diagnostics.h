#pragma once
#ifdef NGB_IOS_PERFORMANCE_DIAGNOSTICS
#include "performance_diagnostics.h"
using NgbMotionDiagnostics=NgbPerformanceDiagnostics;
#else
// Diagnostic-only, render-thread summaries. No image captures, GPU queries or waits.
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstdint>
#include <cstdarg>
#include <utility>
#include <string>
#include <vector>
extern "C" int ngb_diag_writer_start(const char*);
extern "C" int ngb_diag_writer_active(void);
extern "C" int ngb_diag_writer_enqueue(const char*,size_t);
class NgbMotionDiagnostics {
public:
 static int64_t Clock(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 enum Phase { EndpointDraw, ReplayDraw, MidpointGraph, PhaseCount };
 struct Timer {NgbMotionDiagnostics& d;Phase p;int64_t start;Timer(NgbMotionDiagnostics& d,Phase p):d(d),p(p),start(d.Enabled()?Clock():0){}~Timer(){if(start)d.cpu_[p]+=double(Clock()-start)/1e6;}};
 // Start only from the first completed game frame, never constructors/Draw.
 bool Enabled(){return opened_&&ngb_diag_writer_active();}
 void Start(){if(opened_)return;opened_=true;const char *path=std::getenv("NGB_MOTION_DIAGNOSTICS_FILE");if(path)ngb_diag_writer_start(path);}
 void Draw(bool generic){if(Enabled()){draws_++;generic_+=generic;}}
 void Water(uint64_t key,uint32_t modes,unsigned signed_mask,unsigned target_mask,unsigned shadow_mask,bool generic,bool blend,unsigned depth){
  if(!Enabled())return;
  water_++;
  for(auto& w:waters_)if(w.count&&w.key==key){w.count++;w.generic+=generic;return;}
  for(auto& w:waters_)if(!w.count){w={key,modes,signed_mask,target_mask,shadow_mask,1,unsigned(generic),unsigned(blend),depth};return;}
  overflow_++;
 }
 // Bounded CPU state only, emitted once per variant. No texture readback or
 // frame wait. Runtime shader words stay in the user's private diagnostic log.
 void WaterState(uint64_t pixel,uint64_t vertex,bool programmable,const uint32_t* words,size_t count,unsigned tc,unsigned transform,unsigned projection,unsigned width,unsigned height,bool override_position,bool feedback,int clip_instruction=-1,int clip_register=-1){
  if(!Enabled())return;
  for(const auto& row:water_states_)if(row.used&&row.pixel==pixel&&row.vertex==vertex&&row.tc==tc&&row.transform==transform&&row.projection==projection&&row.width==width&&row.height==height&&row.feedback==feedback&&row.programmable==programmable&&row.override_position==override_position)return;
  for(auto& row:water_states_)if(!row.used){
   row.used=true;row.pixel=pixel;row.vertex=vertex;row.programmable=programmable;row.tc=tc;row.transform=transform;row.projection=projection;row.width=width;row.height=height;row.override_position=override_position;row.feedback=feedback;row.total_words=count;row.clip_instruction=clip_instruction;row.clip_register=clip_register;
   if(words&&count)row.words.assign(words,words+std::min<size_t>(count,1024));
   return;
  }
 }
 void MarkReplayPacket(const void* packet,uint64_t key){if(!Enabled())return;if(packet_count_<packets_.size())packets_[packet_count_++]={packet,key};else packet_drops_++;}
 void ReplayPacket(const void* packet){if(!Enabled())return;for(unsigned i=0;i<packet_count_;i++)if(packets_[i].first==packet){ReplayWater(packets_[i].second);return;}}
 void ResetPackets(){packet_count_=0;}
 bool ReportDue(int64_t now){return Enabled()&&first_&&now-window_>=1000000000LL;}
 void SceneInput(uint64_t bytes,unsigned hits,uint64_t cache_bytes=0,unsigned evictions=0,unsigned fixed=0){if(Enabled()){input_bytes_+=bytes;input_hits_+=hits;cache_bytes_=std::max(cache_bytes_,cache_bytes);evictions_+=evictions;fixed_+=fixed;}}
 void TargetSample(bool replay){if(Enabled())(replay?target_replay_:target_readonly_)++;}
 void SceneWork(double proof_ms,unsigned proofs,unsigned hits,unsigned vertices,unsigned interpreted,unsigned skin){if(Enabled()){proof_ms_+=proof_ms;proofs_+=proofs;proof_hits_+=hits;vertices_+=vertices;interpreted_+=interpreted;skin_+=skin;}}
 void ReplayWater(uint64_t key){if(!Enabled())return;water_replay_++;for(auto& w:waters_)if(w.count&&w.key==key){w.replays++;return;}}
 void Frame(int64_t now,unsigned frame,int requested,bool wanted,bool reset,unsigned scene,unsigned changed,unsigned equal,uint64_t packet,uint64_t snapshots,double extract_ms,double interpolate_ms,double project_ms,uint64_t mids,uint64_t ends,uint64_t superseded,double display_hz=0,unsigned width=0,unsigned height=0,unsigned msaa=0,bool capture_valid=true){
  Start();
  if(!Enabled())return;
  if(!first_){first_=last_=window_=now;return;}
  double dt=double(now-last_)/1e6;last_=now;
  frames_++;peak_=std::max(peak_,dt);for(unsigned i=0;i<4;i++)slow_[i]+=dt>limits_[i];
  resets_+=reset;unwanted_+=!wanted;invalid_+=!capture_valid;scene_+=scene;changed_+=changed;equal_+=equal;packet_=std::max(packet_,packet);snapshots_=std::max(snapshots_,snapshots);
  extract_+=extract_ms;interpolate_+=interpolate_ms;project_+=project_ms;
  if(now-window_<1000000000LL)return;
  double seconds=double(now-window_)/1e9;
  std::string report;
  for(auto& row:water_states_)if(row.used&&!row.reported){
   Append(report,"[water-state] pixel=%016llx vertex=%016llx programmable=%u viewport=%ux%u tc2=%08x transform2=%08x projection=%x override=%u feedback=%u words=%zu clip_instruction=%d clip_register=%d\n",(unsigned long long)row.pixel,(unsigned long long)row.vertex,unsigned(row.programmable),row.width,row.height,row.tc,row.transform,row.projection,unsigned(row.override_position),unsigned(row.feedback),row.total_words,row.clip_instruction,row.clip_register);
   if(!row.words.empty()){
    Append(report,"[water-program] vertex=%016llx captured=%zu data=",(unsigned long long)row.vertex,row.words.size());
    for(auto word:row.words)Append(report,"%08x ",word);
    report+='\n';
   }
   row.reported=true;
  }
  Append(report,"[scene-work] frame=%u proof_ms=%.3f proofs=%u proof_hits=%u vertices=%u interpreted=%u skinned=%u\n",frame,proof_ms_,proofs_,proof_hits_,vertices_,interpreted_,skin_);
  proof_ms_=0;proofs_=proof_hits_=vertices_=interpreted_=skin_=0;
  Append(report,"[scene-input] frame=%u key_MiB=%.3f hits=%u target_readonly=%u target_replay=%u cache_MiB=%.3f evictions=%u fixed=%u\n",frame,input_bytes_/1048576.,input_hits_,target_readonly_,target_replay_,cache_bytes_/1048576.,evictions_,fixed_);
  input_bytes_=cache_bytes_=0;input_hits_=target_readonly_=target_replay_=evictions_=fixed_=0;
  Append(report,"[frame] t=%.3f unix=%lld frame=%u game_hz=%.2f display_hz=%.1f target=%ux%u msaa=%u peak_ms=%.2f over20=%u over33=%u over50=%u over100=%u requested=%d wanted=%d resets=%u ineligible=%u invalid_capture=%u draws=%u generic=%u scene=%u changed=%u equal=%u packet_peak_MiB=%.2f snapshots_peak_MiB=%.2f endpoint_cpu_ms=%.2f replay_cpu_ms=%.2f midpoint_inclusive_cpu_ms=%.2f extract_ms=%.2f interpolate_ms=%.2f project_ms=%.2f midpoint_total=%llu endpoint_total=%llu superseded_total=%llu bump_draws=%u bump_replays=%u bump_overflow=%u log_dropped=%u packet_metadata_dropped=%u\n",double(now-first_)/1e9,(long long)std::time(nullptr),frame,frames_/seconds,display_hz,width,height,msaa,peak_,slow_[0],slow_[1],slow_[2],slow_[3],requested,int(wanted),resets_,unwanted_,invalid_,draws_,generic_,scene_,changed_,equal_,packet_/1048576.,snapshots_/1048576.,cpu_[0],cpu_[1],cpu_[2],extract_,interpolate_,project_,(unsigned long long)mids,(unsigned long long)ends,(unsigned long long)superseded,water_,water_replay_,overflow_,dropped_,packet_drops_);
  for(const auto& w:waters_)if(w.count)Append(report,"[bump] frame=%u shader=%016llx modes=%08x signed=%x target=%x shadow=%x blend=%u depth=%x draws=%u generic=%u replays=%u\n",frame,(unsigned long long)w.key,w.modes,w.signed_mask,w.target_mask,w.shadow_mask,w.blend,w.depth,w.count,w.generic,w.replays);
  frames_=draws_=generic_=water_=water_replay_=overflow_=resets_=unwanted_=invalid_=scene_=changed_=equal_=0;peak_=extract_=interpolate_=project_=0;packet_=snapshots_=0;cpu_={};slow_={};waters_={};window_=now;
  Enqueue(std::move(report));
 }
private:
 struct WaterRow{uint64_t key=0;uint32_t modes=0;unsigned signed_mask=0,target_mask=0,shadow_mask=0,count=0,generic=0,blend=0,depth=0,replays=0;};
 struct WaterStateRow{uint64_t pixel=0,vertex=0;unsigned tc=0,transform=0,projection=0,width=0,height=0;bool used=false,reported=false,programmable=false,override_position=false,feedback=false;size_t total_words=0;int clip_instruction=-1,clip_register=-1;std::vector<uint32_t> words;};
 static void Append(std::string& text,const char* format,...){
  char row[4096];va_list args;va_start(args,format);int n=std::vsnprintf(row,sizeof row,format,args);va_end(args);
  if(n>0)text.append(row,std::min<size_t>(size_t(n),sizeof row-1));
 }
 // Native serial GCD writer uses only runtime APIs already present in the port.
 void Enqueue(std::string text){if(!ngb_diag_writer_enqueue(text.data(),text.size()))dropped_++;}
 unsigned dropped_=0;
 uint64_t input_bytes_=0,cache_bytes_=0;unsigned input_hits_=0,target_readonly_=0,target_replay_=0,evictions_=0,fixed_=0;
 double proof_ms_=0;unsigned proofs_=0,proof_hits_=0,vertices_=0,interpreted_=0,skin_=0;
 std::array<std::pair<const void*,uint64_t>,64> packets_{};unsigned packet_count_=0,packet_drops_=0;
 bool opened_=false;int64_t first_=0,last_=0,window_=0;
 unsigned frames_=0,draws_=0,generic_=0,water_=0,water_replay_=0,overflow_=0,resets_=0,unwanted_=0,invalid_=0,scene_=0,changed_=0,equal_=0;
 double peak_=0,extract_=0,interpolate_=0,project_=0;uint64_t packet_=0,snapshots_=0;
 std::array<double,PhaseCount> cpu_{};std::array<unsigned,4> slow_{};std::array<WaterRow,8> waters_{};
 std::array<WaterStateRow,8> water_states_{};
 static constexpr double limits_[4]={20,33.333,50,100};
};

#endif
