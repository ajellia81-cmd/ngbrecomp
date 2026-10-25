#!/usr/bin/env python3
"""Exercise the real presenter publication functions with a blocked GPU stub.

No Vulkan device, game data, SDL window, settings or audio are used. A second
producer attempts publication while the pair worker owns the presentation lock.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'xboxrecomp/src/nv2a_vk/host/present_sdl.c'


def function(source, name):
    match = re.search(r'^static [^\n;]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    if not match:
        raise ValueError('Missing production function: ' + name)
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


PREFIX = r'''
#define _DEFAULT_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "nv2a_vk/host/pair_mailbox.h"
typedef uintptr_t VkImage;
#define VK_NULL_HANDLE 0
typedef struct { struct { int width,height; VkImage image; } display; } PGRAPHVkState;
typedef struct { int unused; } PGRAPHState;
static PGRAPHState graph, *s_pg=&graph;
static PGRAPHVkState renderer;
static pthread_mutex_t s_present_mu=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_pair_mu=PTHREAD_MUTEX_INITIALIZER;
static NgbPairMailbox s_pairs;
static VkImage s_ext_image;
static uint32_t s_ext_w,s_ext_h;
static int64_t s_ext_set_us;
static unsigned nv2a_ext_pictures,nv2a_ext_presented;
static unsigned long nv2a_host_presents;
static bool s_external_wait,s_pair_render;
static int s_quality_set,phase,failed;
static int64_t g_get_monotonic_time(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;
}
static void present_locked(PGRAPHState *,bool);
'''

SUFFIX = r'''
static void present_locked(PGRAPHState *pg,bool pfifo) {
    (void)pg; (void)pfifo;
    if (__atomic_exchange_n(&phase,1,__ATOMIC_ACQ_REL)==0) {
        while (__atomic_load_n(&phase,__ATOMIC_ACQUIRE)!=2) usleep(100);
        /* Give the competing producer time to attempt publication. Its whole
         * descriptor must remain hidden until this source has been consumed. */
        for (int i=0;i<200;i++) {
            int w,h; bool external;
            VkImage image=picture_source(&renderer,&w,&h,&external);
            if (!external || image!=11 || w!=640 || h!=480) {
                failed=1; break;
            }
            usleep(100);
        }
    }
    nv2a_host_presents++;
}
static void *worker(void *unused) {
    (void)unused;
    NgbPresentPair p={.mid=11,.end=12,.w=640,.h=480};
    pair_present(&p,true);
    return NULL;
}
static void *producer(void *unused) {
    (void)unused;
    while (__atomic_load_n(&phase,__ATOMIC_ACQUIRE)!=1) usleep(100);
    __atomic_store_n(&phase,2,__ATOMIC_RELEASE);
    set_external(21,3000,1400,false);
    return NULL;
}
int main(void) {
    pthread_t a,b;
    if (pthread_create(&a,NULL,worker,NULL) || pthread_create(&b,NULL,producer,NULL)) return 2;
    pthread_join(a,NULL); pthread_join(b,NULL);
    if (failed || s_ext_image!=21 || s_ext_w!=3000 || s_ext_h!=1400) return 1;
    return 0;
}
'''


def main():
    source = SOURCE.read_text()
    publish = function(source, 'set_external')
    rest = '\n'.join(function(source, name) for name in ('picture_source', 'pair_present'))
    lock = 'pthread_mutex_lock(&s_present_mu);'
    anchor = 'PGRAPHState *pg=s_pg;'
    if publish.count(lock) != 1 or publish.count(anchor) != 1:
        raise ValueError('Publication boundary changed; review negative control')
    # Reintroduce the previous unlocked publication to ensure the race is caught.
    unsafe = publish.replace(lock, '', 1).replace(anchor, lock + '\n' + anchor, 1)
    with tempfile.TemporaryDirectory(prefix='ngb-present-test-') as directory:
        root = Path(directory)
        for name, body, expected in [('fixed', publish, 0), ('unsafe', unsafe, 1)]:
            path = root / (name + '.c')
            path.write_text(PREFIX + rest + '\n' + body + SUFFIX)
            binary = root / name
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra',
                            '-Werror', '-pthread', '-I' + str(ROOT / 'xboxrecomp/src'),
                            str(path), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], timeout=5)
            if result.returncode != expected:
                raise RuntimeError(name + ': unexpected race test result ' + str(result.returncode))
    print('Presenter image/size publication: serialized producers PASS; unsafe control rejected')


if __name__ == '__main__':
    main()
