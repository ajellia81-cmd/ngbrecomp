#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
const struct mach_header *ngb_probe_game_header(void);
const struct mach_header *ngb_test_dyld_get_image_header(uint32_t);
int main(void){
    Dl_info image;assert(dladdr((void *)ngb_probe_game_header,&image));
    const struct mach_header *host=_dyld_get_image_header(0);
    assert(host && host!=image.dli_fbase);
    const struct mach_header *game=ngb_probe_game_header();assert(game==image.dli_fbase);
    assert(ngb_test_dyld_get_image_header(0)==host);
    assert(ngb_test_dyld_get_image_header(1)==_dyld_get_image_header(1));
    unsigned long size=0;
    const uint8_t *text=getsegmentdata((const struct mach_header_64 *)game,"__TEXT",&size);
    assert(text && size && (uintptr_t)ngb_probe_game_header>=(uintptr_t)text &&
        (uintptr_t)ngb_probe_game_header<(uintptr_t)text+size);
    puts("Mac executable identity: loaded game text selected; host and dyld enumeration preserved");
    return 0;
}
