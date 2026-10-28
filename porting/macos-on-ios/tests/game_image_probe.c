#include <mach-o/dyld.h>
const struct mach_header *ngb_test_dyld_get_image_header(uint32_t);
__attribute__((visibility("default"),noinline))
const struct mach_header *ngb_probe_game_header(void) {
    // Prevent a tail call: the compatibility layer must see the game caller.
    const struct mach_header *volatile h=ngb_test_dyld_get_image_header(0);return h;
}
