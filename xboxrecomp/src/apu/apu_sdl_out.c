/*
 * Host audio output through SDL2, for POSIX builds and for Windows builds
 * with XBOX_SDL_BACKENDS (otherwise Windows has XAudio2).
 *
 * The APU thread renders 48 kHz stereo 16-bit blocks at the console's pace
 * and queues them here. Kept apart from apu_core.c, whose QEMU shim defines
 * SDL-flavoured stand-ins that would clash with the real header.
 */
#if !defined(_WIN32) || defined(XBOX_SDL_BACKENDS)
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "apu_sdl_out.h"

static SDL_AudioDeviceID s_dev;
static int s_failed;
static int s_playing;
static int s_paused;
static SDL_SpinLock s_state_lock;

int apu_sdl_out_init(void)
{
    SDL_AudioSpec want, have;
    const char *e = getenv("NGB_AUDIO");

    if (s_dev)
        return 1;
    /* NGB_AUDIO=0 turns sound off. */
    if (s_failed || (e && *e == '0'))
        return 0;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[APU] SDL audio unavailable: %s\n", SDL_GetError());
        s_failed = 1;
        return 0;
    }
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 480;     /* 10 ms: the device's own share of the delay */
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!device) {
        fprintf(stderr, "[APU] cannot open an audio device: %s\n", SDL_GetError());
        s_failed = 1;
        return 0;
    }
    SDL_AtomicLock(&s_state_lock);
    s_dev = device;
    SDL_AtomicUnlock(&s_state_lock);
    /* Starts paused; apu_sdl_out_submit unpauses once a little is queued. */
    fprintf(stderr, "[APU] SDL audio: %s, 48 kHz stereo\n",
            SDL_GetCurrentAudioDriver());
    return 1;
}

int apu_sdl_out_active(void)
{
    SDL_AtomicLock(&s_state_lock);
    int active = s_dev != 0;
    SDL_AtomicUnlock(&s_state_lock);
    return active;
}

/* Silence while paused: what is queued is dropped, and the device waits for
 * a little to be queued again before it plays (as at the start). */
void apu_sdl_out_pause(int paused)
{
    SDL_AtomicLock(&s_state_lock);
    s_paused = !!paused;
    if (s_dev) {
        SDL_PauseAudioDevice(s_dev, 1);
        SDL_ClearQueuedAudio(s_dev);
    }
    s_playing = 0;
    SDL_AtomicUnlock(&s_state_lock);
}

int apu_sdl_out_queued_frames(void)
{
    SDL_AtomicLock(&s_state_lock);
    int queued = s_dev && s_playing ? (int)(SDL_GetQueuedAudioSize(s_dev) / 4) : -1;
    SDL_AtomicUnlock(&s_state_lock);
    return queued;
}

/* Queue n stereo frames. The APU paces itself on the queue level (see
 * throttle in apu_core.c), so this only drops a backlog that has grown far
 * past it -- a suspended stream -- rather than play it late. */
void apu_sdl_out_submit(const int16_t *frames, int n)
{
    const Uint32 max_queued = 48000 / 2 * 4;   /* 500 ms of S16 stereo */
    const Uint32 prebuffer = 1536 * 4;          /* ~32 ms */
    Uint32 queued;

    SDL_AtomicLock(&s_state_lock);
    if (!s_dev || s_paused) {
        SDL_AtomicUnlock(&s_state_lock);
        return;
    }
    queued = SDL_GetQueuedAudioSize(s_dev);
    if (queued > max_queued)
        SDL_ClearQueuedAudio(s_dev);
    SDL_QueueAudio(s_dev, frames, (Uint32)n * 4);
    if (!s_playing && SDL_GetQueuedAudioSize(s_dev) >= prebuffer) {
        SDL_PauseAudioDevice(s_dev, 0);
        s_playing = 1;
    }
    SDL_AtomicUnlock(&s_state_lock);
}
#endif
