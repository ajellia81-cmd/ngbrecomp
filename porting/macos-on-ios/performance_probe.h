#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void ngb_perf_context(int *thermal, int *low_power, int *foreground);
void ngb_perf_start(const char *path);
void ngb_perf_stop(void);
uint64_t ngb_perf_cpu_us(void);
void ngb_perf_mode(int mode, int hz);
void ngb_perf_fps(uint64_t game, uint64_t shown, uint64_t elapsed_us, uint64_t game_gap_us, uint64_t shown_gap_us);
void ngb_perf_match(int camera_cut, uint64_t matched, uint64_t draws);
void ngb_perf_interp(uint64_t cpu_us, uint64_t wall_us, uint64_t replays, int pictures, int complete);
void ngb_perf_gpu_support(int supported);
void ngb_perf_gpu(uint64_t ns);
void ngb_perf_wait(uint64_t us);
void ngb_perf_backend(uint64_t compile_us, uint64_t shaders, uint64_t pipelines, uint64_t draws, uint64_t drawn, uint64_t missing, int replay);
void ngb_perf_async(uint64_t us, int pipeline);
void ngb_perf_queue(uint64_t remaining);
#ifdef __cplusplus
}
#endif
