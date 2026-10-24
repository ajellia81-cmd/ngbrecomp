/* Synthetic screen sizes exercise the actual settings and viewport math;
 * these numbers are not claims about any iPhone model's specifications. */
#include "launcher.h"
#include "nv2a_vk/host/native_viewport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int screen_w = 3000, screen_h = 1400;
void ngb_ios_screen_size(int *w, int *h) { *w = screen_w; *h = screen_h; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "native settings check failed at %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
    char scratch[] = "/tmp/ngb-native-settings-XXXXXX";
    CHECK(mkdtemp(scratch) != NULL);
    setenv("NGB_CONFIG_DIR", scratch, 1);
    NgbSettings settings;
    ngb_settings_load(&settings);
    settings.aspect = ASPECT_21_9;
    ngb_settings_change(&settings, 0, 1);
    CHECK(settings.aspect == ASPECT_IPHONE);
    CHECK(settings.width == screen_w && settings.height == screen_h);
    CHECK(fabs(ngb_settings_aspect_ratio(&settings) - (double)screen_w / screen_h) < 1e-9);
    settings.scale = 10;
    ngb_settings_change(&settings, 3, 1);
    CHECK(settings.scale == NGB_RENDER_SCALE_IPHONE_NATIVE);
    CHECK(ngb_settings_runtime_scale(&settings) == 3);
    ngb_settings_save(&settings);
    ngb_settings_load(&settings);
    CHECK(settings.scale == NGB_RENDER_SCALE_IPHONE_NATIVE && settings.aspect == ASPECT_IPHONE);
    screen_w = 2400; screen_h = 1080;
    ngb_settings_snap_size(&settings);
    CHECK(settings.width == screen_w && settings.height == screen_h);
    uint32_t w, h;
    ngb_native_viewport(screen_w, screen_h, ngb_settings_aspect_ratio(&settings), &w, &h);
    CHECK(w == 2400 && h == 1080);
    const double aspects[] = {4.0/3, 16.0/9, 16.0/10, 64.0/27};
    for (unsigned i = 0; i < sizeof aspects / sizeof aspects[0]; i++) {
        ngb_native_viewport(screen_w, screen_h, aspects[i], &w, &h);
        CHECK(w && h && w <= (unsigned)screen_w && h <= (unsigned)screen_h);
        CHECK(fabs((double)w / h - aspects[i]) < 0.002);
    }
    ngb_settings_change(&settings, 3, 1);
    CHECK(settings.scale == 1);
    ngb_settings_change(&settings, 0, 1);
    CHECK(settings.aspect == ASPECT_4_3);
    char path[1024]; snprintf(path, sizeof path, "%s/settings.ini", scratch);
    unlink(path); rmdir(scratch);
    puts("Native aspect/scale append, persistence, screen changes and viewport fitting: OK");
    return 0;
}
