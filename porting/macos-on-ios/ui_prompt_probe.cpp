/* UI diagnostics only: fixed records and low-rate liveness, never GPU timing. */
#include "ui_prompt_probe.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <mutex>
#include <thread>
namespace {
struct State {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    std::array<NgbUiPromptRecord,512> queue{};
    unsigned head=0,tail=0,size=0;
    std::atomic<bool> running{false};
    std::array<std::atomic<uint64_t>,3> counts{};
    FILE *file=nullptr;
    ~State(){if(worker.joinable()){running=false;wake.notify_one();worker.join();}if(file)std::fclose(file);}
};
State &state(){static State s;return s;}
double ui_number(float v){return std::isfinite(v)?double(v):0.0;}
void summary(State &s){
    std::fprintf(s.file,"{\"kind\":\"heartbeat\",\"visits\":%llu,\"footer_candidates\":%llu,\"records\":%llu}\n",
        (unsigned long long)s.counts[0].load(),(unsigned long long)s.counts[1].load(),(unsigned long long)s.counts[2].load());
    std::fflush(s.file);
}
void write(State &s,const NgbUiPromptRecord &r){
    if(r.loading){
        std::fprintf(s.file,"{\"kind\":\"loading\",\"frame\":%u,\"ms\":%u,\"phase\":%u,\"ticks\":%u,\"prompt\":%u,\"guide_seen\":%u,\"pending\":%u,\"screen_mode\":%u,\"fade_bits\":%u,\"card_ticks\":%u,\"card_counter\":%u,\"card_block\":%u,\"card_ready\":%u,\"host_buttons\":%u,\"host_flags\":%u,\"pad_index\":%u,\"guest\":[%u,%u,%u,%u]}\n",
            r.frame,r.wall_ms,r.phase,r.ticks,r.prompt,r.guide,r.pending,r.screen_mode,r.fade_bits,r.card_ticks,r.card_counter,r.card_block,r.card_ready,r.host_buttons,r.host_flags,r.pad_index,
            r.guest[0],r.guest[1],r.guest[2],r.guest[3]);return;
    }
    std::fprintf(s.file,"{\"kind\":\"%s\",\"frame\":%u,\"sprite\":%u,\"texture\":%u,\"caller\":%u,\"parent\":%u,\"ascii\":%d,\"alpha\":%.6f,\"mode\":%d,\"depth\":%d,\"sample\":%u,\"tab\":%d,\"visible\":%d,\"header_visible\":%d,\"state_flags\":%u,\"input\":[%d,%d],\"texture_size\":[%d,%d],\"rect\":[%.3f,%.3f,%.3f,%.3f],\"uv\":[%.6f,%.6f,%.6f,%.6f]}\n",
      r.menu?"menu":"sprite",r.frame,r.sprite,r.texture,r.caller,r.parent,r.letter,ui_number(r.alpha),r.mode,r.depth,r.sample,r.tab,r.visible,r.header_visible,r.state_flags,r.input_x,r.input_y,r.tw,r.th,
      ui_number(r.rect[0]),ui_number(r.rect[1]),ui_number(r.rect[2]),ui_number(r.rect[3]),
      ui_number(r.uv[0]),ui_number(r.uv[1]),ui_number(r.uv[2]),ui_number(r.uv[3]));
}
}
extern "C" void ngb_ui_prompt_stop(void){
    auto &s=state();s.running=false;s.wake.notify_one();if(s.worker.joinable())s.worker.join();
    if(s.file){std::fclose(s.file);s.file=nullptr;}
}
extern "C" int ngb_ui_prompt_start(const char *path){
    ngb_ui_prompt_stop();auto &s=state();s.head=s.tail=s.size=0;for(auto &n:s.counts)n=0;
    if(!path || !(s.file=std::fopen(path,"w"))){std::fprintf(stderr,"[UI probe] Log open failed (errno %d)\n",errno);return 0;}
    std::fputs("{\"session\":true,\"format\":2,\"capture\":\"ui_prompt\",\"max_records\":512,\"performance_observers\":false}\n",s.file);
    std::fflush(s.file); // The file exists even when no sprite reaches the observer.
    s.running=true;
    try{s.worker=std::thread([&s]{
        auto last=std::chrono::steady_clock::now();
        for(;;){
            NgbUiPromptRecord r{};bool have=false;
            {std::unique_lock<std::mutex> lock(s.mutex);
             s.wake.wait_until(lock,last+std::chrono::seconds(1),[&]{return s.size || !s.running.load();});
             if(s.size){r=s.queue[s.tail];s.tail=(s.tail+1)%s.queue.size();s.size--;have=true;}
             else if(!s.running)break;}
            if(have)write(s,r);
            auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(1)){summary(s);last=now;}
        }
        summary(s);
    });}catch(...){std::fputs("{\"worker_error\":true}\n",s.file);std::fflush(s.file);s.running=false;return 0;}
    static bool registered=false;if(!registered){std::atexit(ngb_ui_prompt_stop);registered=true;}return 1;
}
extern "C" void ngb_ui_prompt_count(unsigned stage){auto &s=state();if(stage<3 && s.running.load(std::memory_order_relaxed))s.counts[stage].fetch_add(1,std::memory_order_relaxed);}
extern "C" void ngb_ui_prompt_record(const NgbUiPromptRecord *r){
    auto &s=state();if(!r || !s.running.load(std::memory_order_relaxed))return;
    {std::lock_guard<std::mutex> lock(s.mutex);if(!s.running || s.size==s.queue.size() || s.counts[2].load(std::memory_order_relaxed)>=512)return;
     s.queue[s.head]=*r;s.head=(s.head+1)%s.queue.size();s.size++;s.counts[2].fetch_add(1,std::memory_order_relaxed);}
    s.wake.notify_one();
}
