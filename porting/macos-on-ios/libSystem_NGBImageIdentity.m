// A converted game can be loaded as a library by LiveContainer. The Mac
// scheduler asks for image zero to delimit its own executable code; image zero
// is then the container, so its signal handler never preempts guest code.
// Keep process-wide dyld enumeration unchanged for every other caller.
#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#if defined(NGB_IMAGE_IDENTITY_TEST)
#define _dyld_get_image_header ngb_test_dyld_get_image_header
#endif

static const struct mach_header *(*RealHeader)(uint32_t);
static pthread_once_t headerOnce=PTHREAD_ONCE_INIT;
static void LoadHeaderFunction(void) {
    RealHeader=dlsym(dlopen("/usr/lib/libSystem.B.dylib",RTLD_LAZY),"_dyld_get_image_header");
}
__attribute__((visibility("default")))
const struct mach_header *_dyld_get_image_header(uint32_t index) {
    pthread_once(&headerOnce,LoadHeaderFunction);
    const struct mach_header *original=RealHeader ? RealHeader(index) : NULL;
    if(index!=0)return original;
    void *caller=__builtin_extract_return_addr(__builtin_return_address(0));
    Dl_info image;
    if(dladdr(caller,&image) && image.dli_fname && image.dli_fbase){
        const char *base=strrchr(image.dli_fname,'/');base=base ? base+1 : image.dli_fname;
        if(strcmp(base,"NinjaGaidenBlack")==0){
            const struct mach_header *game=image.dli_fbase;
            if(game!=original)fprintf(stderr,"[Startup] executable identity: game image selected for Mac scheduler\n");
            return game;
        }
    }
    return original;
}
