/* Real SDL queued-audio behavior using its silent dummy backend. */
#include "apu/apu_sdl_out.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int16_t silent[4096 * 2] = {0};
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    setenv("NGB_AUDIO", "1", 1);
    if (!apu_sdl_out_init()) return 1;
    apu_sdl_out_submit(silent, 4096);
    if (apu_sdl_out_queued_frames() < 0) return 2;
    apu_sdl_out_pause(1);
    for (int i = 0; i < 8; i++) apu_sdl_out_submit(silent, 4096);
    if (apu_sdl_out_queued_frames() != -1) return 3;
    apu_sdl_out_pause(1);
    apu_sdl_out_pause(0);
    apu_sdl_out_submit(silent, 4096);
    if (apu_sdl_out_queued_frames() < 0) return 4;
    apu_sdl_out_pause(1);
    puts("SDL dummy audio: queued data cannot restart paused output; resume reprimes: OK");
    return 0;
}
