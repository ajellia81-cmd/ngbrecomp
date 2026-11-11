#pragma once
// CPU wall times only. No readbacks, GPU queries, sleeps, or extra GPU waits.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
extern "C" int ngb_diag_writer_start(const char*);
extern "C" int ngb_diag_writer_active(void);
extern "C" int ngb_diag_writer_enqueue(const char*,size_t);
class NgbPerformanceDiagnostics {
public:
 enum Phase { EndpointDraw, ReplayDraw, MidpointGraph, Queue, GpuWait, SyncSubmit, FrameEnd, Cleanup, Resolve, History, PhaseCount };
 static int64_t Clock(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 bool Enabled() const{return opened_ && ngb_diag_writer_active();}
 struct Timer {
  NgbPerformanceDiagnostics& d;Phase phase;int64_t start;
  Timer(NgbPerformanceDiagnostics& d,Phase p):d(d),phase(p),start(p>=MidpointGraph && d.Enabled()?Clock():0){}
  ~Timer(){if(start)d.Note(phase,Clock()-start);}
 };
 // The compiler thread only touches these atomics; never the render-thread state.
 struct WorkerTimer {
  NgbPerformanceDiagnostics& d;unsigned type;int64_t start;
  WorkerTimer(NgbPerformanceDiagnostics& d,bool pipeline):d(d),type(pipeline?1:0),start(d.recording_.load(std::memory_order_relaxed)?Clock():0){}
  ~WorkerTimer(){if(start){auto ns=uint64_t(Clock()-start);d.worker_ns_[type].fetch_add(ns,std::memory_order_relaxed);d.worker_n_[type].fetch_add(1,std::memory_order_relaxed);auto peak=d.worker_peak_[type].load(std::memory_order_relaxed);while(peak<ns && !d.worker_peak_[type].compare_exchange_weak(peak,ns,std::memory_order_relaxed)){} }}
 };
 void Start(){if(opened_)return;opened_=true;const char* p=std::getenv("NGB_MOTION_DIAGNOSTICS_FILE");if(p)ngb_diag_writer_start(p);}
 void Note(Phase p,int64_t ns){if(Enabled() && ns>0)current_[p]+=double(ns)/1e6;}
 void BeginPresent(int64_t now,uint64_t compile_us,unsigned shaders,unsigned pipelines){
  present_start_=now;outside_ms_=last_?double(now-last_)/1e6:0;sync_ms_=compile_us/1000.;shaders_=shaders;pipelines_=pipelines;
 }
 bool ReportDue(int64_t now) const{return Enabled() && first_ && now-window_>=1000000000LL;}
 void Frame(int64_t,unsigned,int requested,bool wanted,bool reset,unsigned scene,unsigned changed,unsigned,uint64_t packet,uint64_t snapshots,double,double interpolate,double project,uint64_t mids,uint64_t ends,uint64_t superseded,double display=0,unsigned w=0,unsigned h=0,unsigned msaa=0,bool valid=true){
  interpolate_+=interpolate;project_+=project;
  requested_=requested;wanted_=wanted;reset_=reset;scene_=scene;changed_=changed;packet_=packet;snapshots_=snapshots;valid_=valid;
  if(mids||ends||superseded){mids_=mids;ends_=ends;superseded_=superseded;}
  display_=display;width_=w;height_=h;msaa_=msaa;
 }
 // No per-draw clocks or shader capture; draw counters are plain additions.
 void Draw(bool generic){draws_++;generic_+=generic;frame_draws_++;frame_generic_+=generic;}
 void SceneInput(uint64_t bytes,unsigned hits,uint64_t cache=0,unsigned evictions=0,unsigned=0){input_bytes_+=bytes;input_hits_+=hits;cache_peak_=std::max(cache_peak_,cache);evictions_+=evictions;}
 void SceneWork(double,unsigned,unsigned,unsigned vertices,unsigned interpreted,unsigned){vertices_+=vertices;interpreted_+=interpreted;}
 void Projection(unsigned hits,unsigned vertices){projection_hits_+=hits;projected_vertices_+=vertices;}
 void TargetSample(bool){}
 void ReplayPacket(const void*){}
 void ResetPackets(){}
 void Complete(int64_t now,unsigned frame){
  Start();if(!Enabled()){recording_.store(false,std::memory_order_relaxed);return;}
  if(!first_){first_=last_=window_=now;current_={};frame_draws_=frame_generic_=draws_=generic_=0;recording_.store(true,std::memory_order_relaxed);return;}
  const double dt=double(now-last_)/1e6;last_=now;
  const double present=present_start_?double(now-present_start_)/1e6:0;
  frames_++;peak_=std::max(peak_,dt);outside_total_+=outside_ms_;present_total_+=present;sync_total_+=sync_ms_;shader_n_+=shaders_;pipeline_n_+=pipelines_;
  resets_+=reset_;invalid_+=!valid_;ineligible_+=!wanted_;log_total_+=previous_log_ms_;log_peak_=std::max(log_peak_,previous_log_ms_);
  for(unsigned i=0;i<PhaseCount;i++){sum_[i]+=current_[i];max_[i]=std::max(max_[i],current_[i]);}
  for(unsigned i=0;i<4;i++)slow_[i]+=dt>limits_[i];
  if(dt>25){
   if(rows_<rows_data_.size())rows_data_[rows_++]={frame,dt,present,outside_ms_,sync_ms_,previous_log_ms_,current_,requested_,wanted_,valid_,scene_,changed_,frame_draws_,frame_generic_};
   else row_drops_++;
  }
  current_={};previous_log_ms_=0;frame_draws_=frame_generic_=0;
  if(now-window_<1000000000LL)return;
  const auto logging_start=Clock();const double seconds=double(now-window_)/1e9;std::string report;report.reserve(8192);
  Append(report,"[performance] t=%.3f unix=%lld frame=%u game_hz=%.2f peak_ms=%.2f over20=%u over33=%u over50=%u over100=%u requested=%d wanted=%u resets=%u invalid=%u ineligible=%u display_hz=%.1f target=%ux%u msaa=%u outside_present_ms=%.2f present_ms=%.2f sync_compile_ms=%.2f sync_shaders=%u sync_pipelines=%u packet_MiB=%.2f snapshots_MiB=%.2f midpoint_total=%llu endpoint_total=%llu superseded_total=%llu previous_logging_ms=%.3f previous_logging_peak_ms=%.3f dropped_batches=%u dropped_slow_rows=%u\n",double(now-first_)/1e9,(long long)std::time(nullptr),frame,frames_/seconds,peak_,slow_[0],slow_[1],slow_[2],slow_[3],requested_,unsigned(wanted_),resets_,invalid_,ineligible_,display_,width_,height_,msaa_,outside_total_,present_total_,sync_total_,shader_n_,pipeline_n_,packet_/1048576.,snapshots_/1048576.,(unsigned long long)mids_,(unsigned long long)ends_,(unsigned long long)superseded_,log_total_,log_peak_,dropped_,row_drops_);
  Append(report,"[scene-cpu] frame=%u draws=%u generic=%u interpolate_ms=%.3f projection_ms=%.3f projection_reused=%u projected_vertices=%u input_MiB=%.3f input_hits=%u cache_MiB=%.3f evictions=%u decoded_vertices=%u interpreted_vertices=%u\n",frame,draws_,generic_,interpolate_,project_,projection_hits_,projected_vertices_,input_bytes_/1048576.,input_hits_,cache_peak_/1048576.,evictions_,vertices_,interpreted_);
  draws_=generic_=projection_hits_=projected_vertices_=input_hits_=evictions_=vertices_=interpreted_=0;input_bytes_=cache_peak_=0;interpolate_=project_=0;
  Append(report,"[phases] frame=%u midpoint_ms=%.2f resolve_ms=%.2f history_ms=%.2f submit_ms=%.2f gpu_fence_wait_ms=%.2f sync_submit_inclusive_ms=%.2f end_frame_inclusive_ms=%.2f cleanup_ms=%.2f max_gpu_fence_wait_ms=%.2f max_cleanup_ms=%.2f\n",frame,sum_[MidpointGraph],sum_[Resolve],sum_[History],sum_[Queue],sum_[GpuWait],sum_[SyncSubmit],sum_[FrameEnd],sum_[Cleanup],max_[GpuWait],max_[Cleanup]);
  for(unsigned i=0;i<2;i++)Append(report,"[compiler-worker] frame=%u kind=%s jobs=%llu elapsed_ms=%.2f peak_ms=%.2f\n",frame,i?"pipeline":"shader",(unsigned long long)worker_n_[i].exchange(0,std::memory_order_relaxed),worker_ns_[i].exchange(0,std::memory_order_relaxed)/1e6,worker_peak_[i].exchange(0,std::memory_order_relaxed)/1e6);
  for(unsigned i=0;i<rows_;i++){
   const auto& r=rows_data_[i];Append(report,"[slow-frame] frame=%u dt_ms=%.2f present_ms=%.2f outside_present_ms=%.2f sync_compile_ms=%.2f midpoint_ms=%.2f resolve_ms=%.2f history_ms=%.2f submit_ms=%.2f gpu_fence_wait_ms=%.2f sync_submit_inclusive_ms=%.2f end_frame_inclusive_ms=%.2f cleanup_ms=%.2f previous_logging_ms=%.3f requested=%d wanted=%u valid=%u scene=%u changed=%u draws=%u generic=%u\n",r.frame,r.dt,r.present,r.outside,r.compile,r.phase[MidpointGraph],r.phase[Resolve],r.phase[History],r.phase[Queue],r.phase[GpuWait],r.phase[SyncSubmit],r.phase[FrameEnd],r.phase[Cleanup],r.logging,r.requested,unsigned(r.wanted),unsigned(r.valid),r.scene,r.changed,r.draws,r.generic);
  }
  if(!ngb_diag_writer_enqueue(report.data(),report.size()))dropped_++;
  previous_log_ms_=double(Clock()-logging_start)/1e6;
  frames_=resets_=invalid_=ineligible_=shader_n_=pipeline_n_=rows_=row_drops_=0;peak_=outside_total_=present_total_=sync_total_=log_total_=log_peak_=0;sum_={};max_={};slow_={};window_=now;
 }
private:
 struct Row{unsigned frame;double dt,present,outside,compile,logging;std::array<double,PhaseCount> phase;int requested;bool wanted,valid;unsigned scene,changed,draws,generic;};
 static void Append(std::string& s,const char* format,...){char row[2048];va_list a;va_start(a,format);int n=std::vsnprintf(row,sizeof row,format,a);va_end(a);if(n>0)s.append(row,std::min(size_t(n),sizeof(row)-1));}
 unsigned draws_=0,generic_=0,frame_draws_=0,frame_generic_=0,projection_hits_=0,projected_vertices_=0,input_hits_=0,evictions_=0,vertices_=0,interpreted_=0;
 uint64_t input_bytes_=0,cache_peak_=0;double interpolate_=0,project_=0;
 bool opened_=false;std::atomic<bool> recording_{false};
 std::array<std::atomic<uint64_t>,2> worker_ns_{},worker_n_{},worker_peak_{};
 int64_t first_=0,last_=0,window_=0,present_start_=0;
 std::array<double,PhaseCount> current_{},sum_{},max_{};
 std::array<unsigned,4> slow_{};static constexpr double limits_[4]={20,33.333,50,100};
 std::array<Row,8> rows_data_{};unsigned rows_=0,row_drops_=0,dropped_=0;
 unsigned frames_=0,resets_=0,invalid_=0,ineligible_=0,shader_n_=0,pipeline_n_=0,shaders_=0,pipelines_=0;
 double outside_ms_=0,sync_ms_=0,peak_=0,outside_total_=0,present_total_=0,sync_total_=0,previous_log_ms_=0,log_total_=0,log_peak_=0;
 int requested_=0;bool wanted_=false,reset_=false,valid_=false;unsigned scene_=0,changed_=0,width_=0,height_=0,msaa_=0;
 uint64_t packet_=0,snapshots_=0,mids_=0,ends_=0,superseded_=0;double display_=0;
};
