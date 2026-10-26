/*
 * launch.c -- one recompiled program handing over to another, as the Xbox reboots into the image a
 * title launches (#18).
 *
 * Ninja Gaiden Black runs the NES Ninja Gaiden (the cabinet in Han's Bar, the main menu's ARCADE NINJA
 * GAIDEN) by XLaunchNewImage("D:\NinjaTrilogy.xbe"), and that program returns the same way to
 * "D:\default.xbe". XAPI fills in the launch data page (the path, the launching title's id and 3072
 * bytes for the next image) and calls HalReturnToFirmware; the runtime hands the page to the hook
 * installed here (xbox_LaunchImageHook), which saves it, points the next program at it
 * (RECOMP_LAUNCH_PAGE: the runtime puts it back where XGetLaunchInfo looks) and starts that program in
 * place of this one: exec on Linux and macOS, a new process and this one ended on Windows. Each image
 * is its own recompiled program, next to this one: NinjaGaidenBlack for default.xbe, NinjaTrilogy for
 * NinjaTrilogy.xbe, NinjaTomoe for NinjaTomoe.xbe (the Japanese arcade, launched instead when the game's
 * voices are Japanese).
 */
#include "ngb_launch.h"
#include "port.h"
#include "nv2a_window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#endif

static char s_game[4096], s_save[4096];
static int s_w, s_h, s_fs;
static unsigned char s_page[0x1000];
static int s_page_set = -1;

void ngb_default_save_dir(char *out, size_t n)
{
    const char *env = getenv("NGB_SAVE_DIR");
    if (env && *env) {
        snprintf(out, n, "%s", env);
        return;
    }
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    snprintf(out, n, "%s/NinjaGaidenBlack", base && *base ? base : ".");
#else
    const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(out, n, "%s/NinjaGaidenBlack", xdg);
    else
#ifdef __APPLE__
        ngb_macos_dir(out, n, "saves", 1);
#else
        snprintf(out, n, "%s/.local/share/NinjaGaidenBlack", home && *home ? home : ".");
#endif
#endif
}

void ngb_launch_log_to_file(const char *save_dir, const char *name)
{
#ifdef NGB_IOS_RELEASE
    (void)save_dir; (void)name;
    return;
#else
    char log[4200];
    snprintf(log, sizeof log, "%s/%s", save_dir, name);
#if defined(_WIN32)
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        return;
    }
    ngb_mkdir(save_dir);
    FILE *f = fopen(log, "w");
    if (f) {
        fclose(f);
        freopen(log, "a", stdout);
        freopen(log, "a", stderr);
    }
#else
    if (getenv("NGB_TEST_HIDDEN") || isatty(STDOUT_FILENO))
        return;
    struct stat st;
    if (fstat(STDOUT_FILENO, &st) == 0 && S_ISREG(st.st_mode))   /* already going to a file */
        return;
    ngb_mkdir(save_dir);
    if (freopen(log, "w", stdout)) {
        dup2(fileno(stdout), STDERR_FILENO);
        setvbuf(stdout, NULL, _IOLBF, 0);
    }
#endif
#endif
}

static void window_from_env(void)
{
    const char *e = getenv("NGB_LAUNCH_WINDOW");
    if (!(e && sscanf(e, "%d %d %d", &s_w, &s_h, &s_fs) == 3 && s_w > 0 && s_h > 0)) {
        s_w = 1280;
        s_h = 960;
        s_fs = 0;
    }
}
int ngb_launch_window_w(void) { if (!s_w) window_from_env(); return s_w; }
int ngb_launch_window_h(void) { if (!s_w) window_from_env(); return s_h; }
int ngb_launch_fullscreen(void) { if (!s_w) window_from_env(); return s_fs; }

const unsigned char *ngb_launch_page(void)
{
    if (s_page_set < 0) {
        const char *p = getenv("RECOMP_LAUNCH_PAGE");
        FILE *f = p && *p ? fopen(p, "rb") : NULL;
        s_page_set = f && fread(s_page, 1, sizeof s_page, f) == sizeof s_page;
        if (f)
            fclose(f);
    }
    return s_page_set ? s_page : NULL;
}

static void set_env(const char *name, const char *value)
{
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

#if !defined(_WIN32)
static unsigned char s_inherited[1024];   /* the descriptors this program was started with */

static void note_inherited(void)
{
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;
    if (!d)
        d = opendir("/dev/fd");
    if (!d)
        return;
    while ((e = readdir(d))) {
        int fd = atoi(e->d_name);
        if (fd > 2 && fd < 1024 && fd != dirfd(d))
            s_inherited[fd] = 1;
    }
    closedir(d);
}

/* What this program opened (the window, the GPU, the sound device) is not the next one's. */
static void close_on_exec_all(void)
{
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;
    if (!d)
        d = opendir("/dev/fd");
    if (!d)
        return;
    while ((e = readdir(d))) {
        int fd = atoi(e->d_name);
        if (fd > 2 && fd != dirfd(d) && !(fd < 1024 && s_inherited[fd]))
            fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
    }
    closedir(d);
}

/* The virtual CPU's preemption signal, pending at the exec, would end the new image (as for Return to
 * Main Menu, src/ingame_menu.c). */
static void quiet_signals(void)
{
    sigset_t all, none;
    struct sigaction ign;
    sigfillset(&all);
    sigemptyset(&none);
    memset(&ign, 0, sizeof ign);
    ign.sa_handler = SIG_IGN;
    pthread_sigmask(SIG_BLOCK, &all, NULL);
#ifdef __APPLE__
    sigaction(SIGVTALRM, &ign, NULL);
    sigaction(SIGURG, &ign, NULL);
#else
    for (int sig = SIGRTMIN; sig <= SIGRTMAX; sig++)
        sigaction(sig, &ign, NULL);
#endif
    pthread_sigmask(SIG_SETMASK, &none, NULL);
}
#endif

/* The program for a guest image path ("D:\NinjaTrilogy.xbe"), next to this one; 0 if there is none. */
static int s_is_trilogy;

static int program_for(const char *path, char *out, size_t n)
{
    /* XAPI writes the device and the file joined by ';': \Device\CdRom0;NinjaTrilogy.xbe */
    const char *name = path;
    for (const char *c = path; *c; c++)
        if (*c == '\\' || *c == ';')
            name = c + 1;
    /* NinjaTrilogy names no image when it hands back (XLaunchNewImage(NULL)): the disc's title. In
     * the game itself an empty path is a reboot to the dashboard, an exit. */
    if (!*name && s_is_trilogy)
        name = "default.xbe";
    const char *prog;
    if (!strcasecmp(name, "default.xbe"))
        prog = "NinjaGaidenBlack";
    else if (!strcasecmp(name, "NinjaTrilogy.xbe"))
        prog = "NinjaTrilogy";
    else if (!strcasecmp(name, "NinjaTomoe.xbe"))   /* the Japanese build: the game's voices set to Japanese */
        prog = "NinjaTomoe";
    else
        return 0;
    long len = ngb_exe_path(out, n);
    if (len <= 0)
        return 0;
    out[len] = 0;
    char *slash = strrchr(out, '/');
#if defined(_WIN32)
    char *bs = strrchr(out, '\\');
    if (!slash || (bs && bs > slash))
        slash = bs;
#endif
    if (!slash)
        return 0;
#if defined(_WIN32)
    snprintf(slash + 1, n - (size_t)(slash + 1 - out), "%s.exe", prog);
#else
    snprintf(slash + 1, n - (size_t)(slash + 1 - out), "%s", prog);
#endif
    return 1;
}

static void launch_image(const char *path, const void *page, size_t len)
{
#ifdef NGB_IOS_APP
    (void)path; (void)page; (void)len;
    fprintf(stderr, "[LAUNCH] Separate arcade executables are unavailable in the iOS app.\n");
    return;
#else
    char prog[4200], file[4200], win[64];
    if (!program_for(path, prog, sizeof prog)) {
        fprintf(stderr, "[LAUNCH] no program for %s\n", path);
        return;
    }
    snprintf(file, sizeof file, "%s/launch-page.bin", s_save);
    FILE *f = fopen(file, "wb");
    if (!f || fwrite(page, 1, len, f) != len) {
        fprintf(stderr, "[LAUNCH] cannot write %s\n", file);
        if (f)
            fclose(f);
        return;
    }
    fclose(f);
    /* The window as the player left it (F11, Alt+Enter, the settings), not as it started. */
    {
        int w = s_w, h = s_h, fs = nv2a_window_mode(&w, &h);
        if (fs >= 0) {
            s_fs = fs;
            if (w > 0 && h > 0) {
                s_w = w;
                s_h = h;
            }
        }
    }
    snprintf(win, sizeof win, "%d %d %d", s_w, s_h, s_fs);
    set_env("RECOMP_LAUNCH_PAGE", file);
    set_env("NGB_GAME_DIR", s_game);
    set_env("NGB_SAVE_DIR", s_save);
    set_env("NGB_LAUNCH_WINDOW", win);
    fprintf(stderr, "[LAUNCH] %s: starting %s (window %s)\n", path, prog, win);
    fflush(stderr);
#if !defined(_WIN32)
    close_on_exec_all();
    quiet_signals();
    char *args[2] = { prog, NULL };
    execv(prog, args);
    fprintf(stderr, "[LAUNCH] could not start %s\n", prog);
#else
    {
        wchar_t wprog[MAX_PATH];
        static wchar_t line[MAX_PATH + 3];
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        if (MultiByteToWideChar(CP_UTF8, 0, prog, -1, wprog, MAX_PATH)) {
            swprintf(line, MAX_PATH + 3, L"\"%ls\"", wprog);
            if (CreateProcessW(wprog, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                TerminateProcess(GetCurrentProcess(), 0);
            }
        }
        fprintf(stderr, "[LAUNCH] could not start %s (error %lu)\n", prog, GetLastError());
    }
#endif
#endif /* !NGB_IOS_APP */
}

void ngb_launch_init(const char *game_dir, const char *save_dir, int width, int height, int fullscreen)
{
    extern void (*xbox_LaunchImageHook)(const char *path, const void *page, size_t len);
    snprintf(s_game, sizeof s_game, "%s", game_dir);
    snprintf(s_save, sizeof s_save, "%s", save_dir);
    s_w = width;
    s_h = height;
    s_fs = fullscreen;
#if !defined(_WIN32)
    note_inherited();
#endif
    xbox_LaunchImageHook = launch_image;
    char self[4200];
    long len = ngb_exe_path(self, sizeof self);
    if (len > 0) {
        self[len] = 0;
        s_is_trilogy = strstr(self, "NinjaTrilogy") != NULL || strstr(self, "NinjaTomoe") != NULL;
    }
}
