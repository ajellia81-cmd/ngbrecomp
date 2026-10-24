/* Load the scheduler and guest spin loop in a second image, as LiveContainer
 * does. A guest spinning without host calls must still let its peer run. */
#ifdef NGB_VCPU_IMAGE_HOST
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    alarm(10);  /* A regression must fail rather than hang the test runner. */
    void *image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    int (*run)(void) = (int (*)(void))dlsym(image, "ngb_vcpu_image_test");
    if (!run) return 2;
    /* The scheduler's detached timer still runs: keep the image loaded. */
    return run();
}
#else
#include "platform/vcpu.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static atomic_int started, progressed, stop;
static atomic_ulong iterations;
static void pause_ms(long milliseconds)
{
    struct timespec t = { milliseconds / 1000, milliseconds % 1000 * 1000000 };
    nanosleep(&t, NULL);
}
static void *spinner(void *unused)
{
    (void)unused;
    vcpu_attach(VCPU_PRIO_NORMAL);
    atomic_store(&started, 1);
    while (!atomic_load(&stop)) {
        (void)vcpu_game_clock_ns();
        atomic_fetch_add(&iterations, 1);
    }
    vcpu_detach();
    return NULL;
}
static void *peer(void *unused)
{
    (void)unused;
    vcpu_attach(VCPU_PRIO_NORMAL);
    for (int i=0;i<100000;i++) (void)vcpu_game_clock_ns();
    atomic_store(&progressed, 1);
    vcpu_detach();
    return NULL;
}
int ngb_vcpu_image_test(void)
{
    pthread_t first, second;
    setenv("RECOMP_VCPU", "1", 1);
    if (pthread_create(&first, NULL, spinner, NULL)) return 2;
    for (int i = 0; i < 200 && !atomic_load(&started); i++) pause_ms(5);
    if (!atomic_load(&started)) return 2;
    if (pthread_create(&second, NULL, peer, NULL)) return 2;
    for (int i = 0; i < 200 && !atomic_load(&progressed); i++) pause_ms(5);
    int ok = atomic_load(&progressed);
    if (ok) {
        int parked = vcpu_set_paused(1);
        unsigned long before = atomic_load(&iterations);
        vcpu_game_clock_pause(1);
        long long frozen = vcpu_game_clock_ns();
        pause_ms(80);
        int still = parked && before == atomic_load(&iterations);
        int clock_still = frozen == vcpu_game_clock_ns();
        /* Duplicate lifecycle notifications must not reset pause accounting. */
        vcpu_game_clock_pause(1);
        vcpu_set_paused(1);
        vcpu_game_clock_pause(0);
        long long resumed = vcpu_game_clock_ns();
        vcpu_set_paused(0);
        for (int i = 0; i < 100 && before == atomic_load(&iterations); i++) pause_ms(5);
        int running = before != atomic_load(&iterations);
        ok = still && clock_still && running && resumed >= frozen && resumed - frozen < 40000000LL;
        printf("Guest lifecycle: parked=%d frozen-clock=%d resumed=%d: %s\n",
               still, clock_still, running, ok ? "OK" : "FAILED");
    }
    atomic_store(&stop, 1);
    pthread_join(first, NULL);
    pthread_join(second, NULL);
    printf("Guest spin-loop preemption from dynamically loaded image: %s\n", ok ? "OK" : "FAILED");
    return !ok;
}
#endif
