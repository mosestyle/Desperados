// libmain.so: the app side of "Desperados Original".
//
// The original Linux build of Desperados (desperados32, the user's own copy) runs in a separate
// process: glibc's loader + Box64 (Box32 mode, 32-bit x86 on the phone's 64-bit ARM CPU), with
// SDL2 and a bridge libGL of our own (all shipped in the APK's native library folder). That
// process has the memory layout to itself, like on a Linux PC.
//
// This side:
//   - starts that process with a socket for the picture and the input, and a pipe for the sound
//   - replays the game's OpenGL commands with the phone's GPU (bridge/replay.c)
//   - turns touches into the game's mouse: tap = click, long press = right click,
//     drag = move the cursor (to scroll, drag to the edge), back button = Escape
//   - plays the game's sound
// Everything the game process prints goes to Desperados/android-log.txt.
#define _GNU_SOURCE
#include <SDL.h>
#include <stdarg.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "replay.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "Desperados", __VA_ARGS__)

static FILE* logFile;
static void say(const char* title, const char* text) {
    LOG("%s: %s", title, text);
    if (logFile) { fprintf(logFile, "%s: %s\n", title, text); fflush(logFile); }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, text, NULL);
}
static void logf_(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    LOG("%s", buf);
    if (logFile) { fprintf(logFile, "[app] %s\n", buf); fflush(logFile); }
}

static int exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// The folder the APK's native libraries were installed to (where we can run programs from).
static void nativeLibDir(char* out, size_t n) {
    Dl_info info;
    out[0] = 0;
    if (dladdr((void*)nativeLibDir, &info) && info.dli_fname) {
        snprintf(out, n, "%s", info.dli_fname);
        char* slash = strrchr(out, '/');
        if (slash) *slash = 0;
    }
}

static int copyAsset(const char* src, const char* dst) {
    SDL_RWops* in = SDL_RWFromFile(src, "rb");
    if (!in) { logf_("missing asset %s", src); return 0; }
    Sint64 size = SDL_RWsize(in);
    struct stat st;
    if (stat(dst, &st) == 0 && st.st_size == size) { SDL_RWclose(in); return 1; }
    FILE* out = fopen(dst, "wb");
    if (!out) { logf_("can't write %s: %s", dst, strerror(errno)); SDL_RWclose(in); return 0; }
    char buf[65536];
    size_t got;
    while ((got = SDL_RWread(in, buf, 1, sizeof buf)) > 0) fwrite(buf, 1, got, out);
    fclose(out);
    SDL_RWclose(in);
    return 1;
}

// Copy a file unless an identical-looking copy (same size, not older) is already there.
static int copyFile(const char* src, const char* dst, mode_t mode) {
    struct stat a, b;
    if (stat(src, &a) != 0) { logf_("can't read %s: %s", src, strerror(errno)); return 0; }
    if (stat(dst, &b) == 0 && b.st_size == a.st_size && b.st_mtime >= a.st_mtime) { chmod(dst, mode); return 1; }
    FILE* in = fopen(src, "rb");
    if (!in) { logf_("can't open %s: %s", src, strerror(errno)); return 0; }
    char tmp[900];
    snprintf(tmp, sizeof tmp, "%s.part", dst);
    FILE* out = fopen(tmp, "wb");
    if (!out) { logf_("can't write %s: %s", tmp, strerror(errno)); fclose(in); return 0; }
    char buf[65536];
    size_t got;
    int ok = 1;
    while ((got = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, got, out) != got) { ok = 0; break; }
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok || rename(tmp, dst) != 0) { logf_("copying %s failed", src); unlink(tmp); return 0; }
    chmod(dst, mode);
    return 1;
}

// glibc and our Linux libraries are packaged as lib*.so files; the game process finds them
// under their real names through symlinks.
static const char* kLinks[][2] = {
    {"ld-linux-aarch64.so.1", "libglibc_ld.so"},
    {"libc.so.6", "libglibc_c.so"},
    {"libm.so.6", "libglibc_m.so"},
    {"libpthread.so.0", "libglibc_pthread.so"},
    {"libdl.so.2", "libglibc_dl.so"},
    {"librt.so.1", "libglibc_rt.so"},
    {"libresolv.so.2", "libglibc_resolv.so"},
    {"libutil.so.1", "libglibc_util.so"},
    {"libgcc_s.so.1", "libglibc_gcc_s.so"},
    {"libSDL2-2.0.so.0", "libchild_sdl2.so"},
    {"libGL.so.1", "libchild_gl.so"},
    {"libEGL.so.1", "libchild_egl.so"},
};

// ---------------------------------------------------------------------------------------------
// sound: the game process writes PCM (after a 16-byte format header) into a pipe

static SDL_AudioDeviceID audioDev;
static volatile int leaving;
static char audioFifo[600];
static int audioThread(void* arg) {
    const char* fifo = arg;
    int fd = open(fifo, O_RDONLY);
    if (fd < 0) { logf_("audio pipe: %s", strerror(errno)); return 0; }
    unsigned char hdr[16];
    size_t got = 0;
    while (got < sizeof hdr) {
        ssize_t r = read(fd, hdr + got, sizeof hdr - got);
        if (r <= 0) { close(fd); return 0; }
        got += (size_t)r;
    }
    if (leaving) { close(fd); return 0; }
    if (memcmp(hdr, "DSAU", 4) != 0) { logf_("audio: bad header"); close(fd); return 0; }
    SDL_AudioSpec want;
    SDL_zero(want);
    memcpy(&want.freq, hdr + 4, 4);
    memcpy(&want.format, hdr + 8, 2);
    want.channels = hdr[10];
    memcpy(&want.samples, hdr + 12, 2);
    audioDev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (!audioDev) { logf_("audio: %s", SDL_GetError()); close(fd); return 0; }
    logf_("audio: %d Hz, format %#x, %d channels", want.freq, want.format, want.channels);
    SDL_PauseAudioDevice(audioDev, 0);
    const Uint32 bytesPerSec = (Uint32)want.freq * want.channels * (SDL_AUDIO_BITSIZE(want.format) / 8);
    char buf[8192];
    for (;;) {
        ssize_t r = read(fd, buf, sizeof buf);
        if (r <= 0 || leaving) break;
        if (SDL_GetQueuedAudioSize(audioDev) > bytesPerSec / 4) continue;  // keep the delay short
        SDL_QueueAudio(audioDev, buf, (Uint32)r);
    }
    close(fd);
    return 0;
}

// ---------------------------------------------------------------------------------------------

static pid_t startGame(const char* game, const char* gameExe, const char* libDir, const char* linkDir, const char* x86Dir,
                       const char* fifo, const char* userDir, const char* rcFile, const char* runDir,
                       int sock, int logFd, int gw, int gh) {
    char ld[600], box[600], exe[600], ldPath[1200];
    snprintf(ld, sizeof ld, "%s/libglibc_ld.so", libDir);
    snprintf(box, sizeof box, "%s/libbox64g.so", libDir);
    snprintf(exe, sizeof exe, "%s", gameExe);
    snprintf(ldPath, sizeof ldPath, "%s", linkDir);
    char e[24][700];
    int n = 0;
    snprintf(e[n++], 700, "LD_LIBRARY_PATH=%s", linkDir);
    snprintf(e[n++], 700, "BOX64_LD_LIBRARY_PATH=%s", x86Dir);
    snprintf(e[n++], 700, "BOX64_LOG=1");
    snprintf(e[n++], 700, "BOX64_SHOWSEGV=1");
    // Box64 needs a settings file: without one it falls back to a shared-memory file, which
    // Android doesn't have, and then crashes while starting (seen as a crash right after it
    // prints the CPU info)
    snprintf(e[n++], 700, "BOX64_RCFILE=%s", rcFile);
    snprintf(e[n++], 700, "BOX64_NOBANNER=0");
    // the translated-code cache maps files as executable, which Android refuses ("Error allocating
    // Dynarec memory: Permission denied")
    snprintf(e[n++], 700, "BOX64_DYNACACHE=0");
    // Box32 otherwise switches the process to a 32-bit address limit and restarts itself, which
    // can't work here (its own program file has no loader of its own on Android) and would leave
    // the process with that limit set
    snprintf(e[n++], 700, "BOX64_NOPERSONA32BITS=1");
    snprintf(e[n++], 700, "DESP_BRIDGE_FD=%d", sock);
    snprintf(e[n++], 700, "DESP_GAME_EXE=%s", exe);
    snprintf(e[n++], 700, "SDL_VIDEODRIVER=offscreen");
    // without the pipe, the disk driver would fill a real file forever: no sound instead
    snprintf(e[n++], 700, "SDL_AUDIODRIVER=%s", fifo[0] ? "disk" : "dummy");
    snprintf(e[n++], 700, "SDL_DISKAUDIOFILE=%s", fifo);
    snprintf(e[n++], 700, "SDL_OFFSCREEN_WIDTH=%d", gw);
    snprintf(e[n++], 700, "SDL_OFFSCREEN_HEIGHT=%d", gh);
    snprintf(e[n++], 700, "HOME=%s", userDir);
    snprintf(e[n++], 700, "XDG_DATA_HOME=%s", userDir);
    snprintf(e[n++], 700, "GLIBC_TUNABLES=glibc.pthread.rseq=0");
    snprintf(e[n++], 700, "PATH=/system/bin");
    snprintf(e[n++], 700, "LANG=C");
    snprintf(e[n++], 700, "XDG_RUNTIME_DIR=%s", runDir);
    char* envp[25];
    for (int i = 0; i < n; ++i) envp[i] = e[i];
    envp[n] = NULL;
    char guard[600];
    snprintf(guard, sizeof guard, "%s/libchild_sigsys.so", libDir);  // see child/sigsys.c
    char* argv[] = {ld, "--library-path", ldPath, "--preload", guard, box, exe, NULL};

    pid_t pid = fork();
    if (pid == 0) {
        // child: only async-signal-safe calls until exec
        dup2(logFd, 1);
        dup2(logFd, 2);
        if (chdir(game) != 0) _exit(126);
        execve(ld, argv, envp);
        _exit(127);
    }
    return pid;
}

// ---------------------------------------------------------------------------------------------
// touch -> the game's mouse

typedef struct {
    SDL_FingerID id;
    int active, dragged, longFired;
    float sx, sy;          // where it went down (screen px)
    float lx, ly;          // where the last drag step was sent from (screen px)
    Uint32 downAt;
} touch_t;

static void toGame(desp_replay* r, const int rect[4], int sh, float sx, float sy, int* gx, int* gy) {
    int gw, gh;
    desp_replay_game_size(r, &gw, &gh);
    // rect is in GL coordinates (origin bottom-left); touches have their origin top-left
    float top = (float)(sh - rect[1] - rect[3]);
    float u = (sx - rect[0]) / (float)rect[2], v = (sy - top) / (float)rect[3];
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    *gx = (int)(u * gw);
    *gy = (int)(v * gh);
}

static void sendMouse(desp_replay* r, int type, int x, int y, int button) {
    desp_input in;
    memset(&in, 0, sizeof in);
    in.type = (uint32_t)type;
    in.x = x;
    in.y = y;
    in.button = (uint32_t)button;
    desp_replay_send_input(r, &in);
}
static void sendKey(desp_replay* r, int down, int scancode, int keycode) {
    desp_input in;
    memset(&in, 0, sizeof in);
    in.type = down ? IN_KEY_DOWN : IN_KEY_UP;
    in.scancode = (uint32_t)scancode;
    in.keycode = (uint32_t)keycode;
    desp_replay_send_input(r, &in);
}

// What the game process's threads are doing (for the log while we wait for its first picture):
// name, state (R running, S sleeping, D disk), CPU time used, and what a sleeping thread waits in.
static void logGameThreads(pid_t pid) {
    char dir[64];
    snprintf(dir, sizeof dir, "/proc/%d/task", (int)pid);
    DIR* d = opendir(dir);
    if (!d) { logf_("game process: %s", strerror(errno)); return; }
    struct dirent* e;
    int shown = 0;
    while ((e = readdir(d)) && shown < 24) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char p[128], buf[512], wchan[64] = "";
        snprintf(p, sizeof p, "%s/%s/stat", dir, e->d_name);
        FILE* f = fopen(p, "r");
        if (!f) continue;
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = 0;
        char* open = strchr(buf, '('), * close = strrchr(buf, ')');
        if (!open || !close) continue;
        *close = 0;
        char state = close[2];
        unsigned long ut = 0, st = 0;
        // fields after the name: state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime
        sscanf(close + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &st);
        snprintf(p, sizeof p, "%s/%s/wchan", dir, e->d_name);
        f = fopen(p, "r");
        if (f) { size_t k = fread(wchan, 1, sizeof wchan - 1, f); wchan[k] = 0; fclose(f); }
        logf_("  thread %s '%s': %c, cpu %.1f s%s%s", e->d_name, open + 1, state, (ut + st) / 100.0,
              wchan[0] && strcmp(wchan, "0") ? ", waiting in " : "", wchan[0] && strcmp(wchan, "0") ? wchan : "");
        ++shown;
    }
    closedir(d);
}

static void bridgeLog(const char* line) {
    LOG("%s", line);
    if (logFile) { fprintf(logFile, "[bridge] %s\n", line); fflush(logFile); }
}

// Everything SDL_main set up, so that every way out cleans up. Android keeps the app's process
// alive after SDL_main returns and runs SDL_main again in it on the next start, so nothing may
// be left behind (a window left open made the next start fail with "Android only supports one
// window").
static SDL_Window* win;
static SDL_GLContext ctx;
static pid_t gamePid = -1;
static desp_replay* replay;

static int leave(int code) {
    leaving = 1;
    if (gamePid > 0) {
        kill(gamePid, SIGKILL);
        waitpid(gamePid, NULL, 0);
        gamePid = -1;
    }
    if (audioFifo[0]) {  // wake the sound thread if it's still waiting for the game to open the pipe
        int fd = open(audioFifo, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
    }
    if (audioDev) { SDL_CloseAudioDevice(audioDev); audioDev = 0; }
    if (replay) { desp_replay_destroy(replay); replay = NULL; }
    desp_replay_set_logger(NULL);
    if (ctx) { SDL_GL_DeleteContext(ctx); ctx = NULL; }
    if (win) { SDL_DestroyWindow(win); win = NULL; }
    if (logFile) { fclose(logFile); logFile = NULL; }
    SDL_Quit();
    return code;
}

int SDL_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    leaving = 0;
    audioFifo[0] = 0;
    signal(SIGPIPE, SIG_IGN);
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) return 1;

    // the game folder (the start screen has made sure the app may read it)
    static const char* candidates[] = {"/storage/emulated/0/Desperados", "/sdcard/Desperados", NULL};
    const char* game = NULL;
    char path[1024];
    for (int i = 0; candidates[i]; ++i) {
        snprintf(path, sizeof path, "%s/desperados32", candidates[i]);
        if (exists(path)) { game = candidates[i]; break; }
    }
    if (!game) {
        say("Desperados",
            "Couldn't find the game.\n\nCopy the Linux version of Desperados (the folder with "
            "'desperados32', 'data', 'bootmenu' and 'shaders') to the phone's internal storage "
            "as a folder named 'Desperados', then start the app again.");
        return leave(0);
    }
    snprintf(path, sizeof path, "%s/android-log.txt", game);
    logFile = fopen(path, "w");
    int logFd = logFile ? fileno(logFile) : -1;
    logf_("Desperados Original: game folder %s", game);
    desp_replay_set_logger(bridgeLog);
    {
        FILE* test = fopen(path, "r");  // can we read the folder at all?
        if (!test) {
            say("Desperados", "The app can't read the Desperados folder. Allow 'all files access' for "
                              "Desperados Original in the phone's settings, then start it again.");
            return leave(0);
        }
        fclose(test);
    }

    // window and GLES 3 context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    win = SDL_CreateWindow("Desperados", 0, 0, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!win) { say("Desperados", SDL_GetError()); return leave(1); }
    ctx = SDL_GL_CreateContext(win);
    if (!ctx) { say("Desperados", SDL_GetError()); return leave(1); }
    SDL_GL_SetSwapInterval(1);
    logf_("GPU: %s, %s", (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
    int sw, sh;
    SDL_GL_GetDrawableSize(win, &sw, &sh);
    // the game draws at the phone's aspect ratio, 768 pixels high
    int gh = 768, gw = sw > 0 && sh > 0 ? (int)((long)768 * sw / sh) & ~1 : 1024;
    if (gw < 1024) { gw = 1024; gh = sh > 0 ? (int)((long)1024 * sh / sw) & ~1 : 768; }
    logf_("screen %dx%d, game window %dx%d", sw, sh, gw, gh);
    // something on the screen straight away
    glViewport(0, 0, sw, sh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(win);

    // runtime files
    char libDir[512], files[512], linkDir[600], x86Dir[600], fifo[600], userDir[600], rcFile[600];
    nativeLibDir(libDir, sizeof libDir);
    snprintf(files, sizeof files, "%s", SDL_AndroidGetInternalStoragePath());
    snprintf(linkDir, sizeof linkDir, "%s/rt", files);
    snprintf(x86Dir, sizeof x86Dir, "%s/x86lib", files);
    snprintf(fifo, sizeof fifo, "%s/audio.pipe", files);
    snprintf(userDir, sizeof userDir, "%s/userdata", game);
    mkdir(files, 0755);
    mkdir(linkDir, 0755);
    mkdir(x86Dir, 0755);
    mkdir(userDir, 0755);
    for (size_t i = 0; i < sizeof kLinks / sizeof kLinks[0]; ++i) {
        char link[800], target[800];
        snprintf(link, sizeof link, "%s/%s", linkDir, kLinks[i][0]);
        snprintf(target, sizeof target, "%s/%s", libDir, kLinks[i][1]);
        unlink(link);
        if (symlink(target, link) != 0) logf_("symlink %s: %s", link, strerror(errno));
        if (!exists(target)) logf_("missing %s", target);
    }
    {
        char dst[800];
        snprintf(dst, sizeof dst, "%s/libstdc++.so.6", x86Dir);
        copyAsset("x86lib/libstdc++.so.6", dst);
        snprintf(dst, sizeof dst, "%s/libgcc_s.so.1", x86Dir);
        copyAsset("x86lib/libgcc_s.so.1", dst);
    }
    // Box64 only runs files marked executable, which nothing on the shared storage is: run a copy
    // of the game's program from the app's own folder (the game still works in its own folder)
    char gameExe[700], srcExe[700];
    snprintf(srcExe, sizeof srcExe, "%s/desperados32", game);
    snprintf(gameExe, sizeof gameExe, "%s/desperados32", files);
    if (!copyFile(srcExe, gameExe, 0755)) {
        say("Desperados", "Couldn't copy desperados32 into the app. The details are in Desperados/android-log.txt.");
        return leave(1);
    }
    snprintf(rcFile, sizeof rcFile, "%s/box64.box64rc", files);
    {
        FILE* f = fopen(rcFile, "w");
        if (f) {
            fputs("# Box64 settings for the game process\n[desperados32]\nBOX64_DYNAREC_BIGBLOCK=1\n", f);
            fclose(f);
        } else logf_("can't write %s: %s", rcFile, strerror(errno));
    }
    unlink(fifo);
    if (mkfifo(fifo, 0600) != 0) { logf_("no sound: can't make the audio pipe (%s)", strerror(errno)); fifo[0] = 0; }
    snprintf(audioFifo, sizeof audioFifo, "%s", fifo);

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) { say("Desperados", "socketpair failed"); return leave(1); }
    // the game's end must survive exec: clear close-on-exec on it only
    fcntl(sv[1], F_SETFD, 0);
    logf_("starting the game process (libraries in %s)", libDir);
    gamePid = startGame(game, gameExe, libDir, linkDir, x86Dir, fifo, userDir, rcFile, files, sv[1], logFd, gw, gh);
    close(sv[1]);
    if (gamePid < 0) { say("Desperados", "Couldn't start the game process."); return leave(1); }
    if (fifo[0]) {
        SDL_Thread* th = SDL_CreateThread(audioThread, "audio", audioFifo);
        if (th) SDL_DetachThread(th);
    }

    replay = desp_replay_create(sv[0]);
    int rect[4] = {0, 0, sw, sh};
    touch_t t = {0};
    int frames = 0, running = 1;
    Uint32 started = SDL_GetTicks(), lastNote = started;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_KEYDOWN: case SDL_KEYUP:
                if (ev.key.keysym.sym == SDLK_AC_BACK) sendKey(replay, ev.type == SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
                else sendKey(replay, ev.type == SDL_KEYDOWN, ev.key.keysym.scancode, ev.key.keysym.sym);
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) SDL_GL_GetDrawableSize(win, &sw, &sh);
                break;
            case SDL_FINGERDOWN:
                if (t.active || !frames) break;  // one finger at a time for now
                t.active = 1;
                t.id = ev.tfinger.fingerId;
                t.dragged = t.longFired = 0;
                t.sx = ev.tfinger.x * sw;
                t.sy = ev.tfinger.y * sh;
                t.downAt = SDL_GetTicks();
                {
                    int gx, gy;
                    toGame(replay, rect, sh, t.sx, t.sy, &gx, &gy);
                    sendMouse(replay, IN_MOUSE_TO, gx, gy, 0);
                }
                break;
            case SDL_FINGERMOTION:
                if (!t.active || ev.tfinger.fingerId != t.id) break;
                {
                    // a drag moves the camera in a mission (the picture follows the finger),
                    // and the cursor in the menus
                    float x = ev.tfinger.x * sw, y = ev.tfinger.y * sh;
                    float dx = x - t.sx, dy = y - t.sy, slop = sh * 0.02f;
                    if (!t.dragged && dx * dx + dy * dy > slop * slop) {
                        t.dragged = 1;
                        t.lx = t.sx;
                        t.ly = t.sy;
                    }
                    if (t.dragged && rect[2] > 0 && rect[3] > 0) {
                        int gw, gh, gx, gy;
                        desp_replay_game_size(replay, &gw, &gh);
                        toGame(replay, rect, sh, x, y, &gx, &gy);
                        desp_input in;
                        memset(&in, 0, sizeof in);
                        in.type = IN_PAN;
                        in.x = gx;
                        in.y = gy;
                        in.dx = (int32_t)((x - t.lx) * gw / rect[2] * 16);
                        in.dy = (int32_t)((y - t.ly) * gh / rect[3] * 16);
                        t.lx = x;
                        t.ly = y;
                        desp_replay_send_input(replay, &in);
                    }
                }
                break;
            case SDL_FINGERUP:
                if (!t.active || ev.tfinger.fingerId != t.id) break;
                t.active = 0;
                if (!t.dragged && !t.longFired) {
                    int gx, gy;
                    toGame(replay, rect, sh, t.sx, t.sy, &gx, &gy);
                    sendMouse(replay, IN_MOUSE_TO, gx, gy, 0);
                    sendMouse(replay, IN_MOUSE_DOWN, gx, gy, SDL_BUTTON_LEFT);
                    sendMouse(replay, IN_MOUSE_UP, gx, gy, SDL_BUTTON_LEFT);
                }
                break;
            default: break;
            }
        }
        // long press: right click
        if (t.active && !t.dragged && !t.longFired && SDL_GetTicks() - t.downAt > 500) {
            int gx, gy;
            toGame(replay, rect, sh, t.sx, t.sy, &gx, &gy);
            sendMouse(replay, IN_MOUSE_TO, gx, gy, 0);
            sendMouse(replay, IN_MOUSE_DOWN, gx, gy, SDL_BUTTON_RIGHT);
            sendMouse(replay, IN_MOUSE_UP, gx, gy, SDL_BUTTON_RIGHT);
            t.longFired = 1;
        }

        int got = desp_replay_frame(replay, frames ? 8 : 16);
        if (got == 0) {
            int status = 0;
            pid_t w = 0;
            for (int i = 0; i < 40 && (w = waitpid(gamePid, &status, WNOHANG)) == 0; ++i) SDL_Delay(50);
            if (w == gamePid) {
                gamePid = -1;
                if (WIFSIGNALED(status))
                    logf_("the game process was stopped by signal %d (%s) after %d frames", WTERMSIG(status),
                          WTERMSIG(status) == SIGSYS ? "a system call Android doesn't allow" : strsignal(WTERMSIG(status)), frames);
                else
                    logf_("the game process ended (exit code %d) after %d frames", WEXITSTATUS(status), frames);
            } else {
                logf_("the game process closed its connection after %d frames", frames);
            }
            say("Desperados", frames ? "The game has ended." :
                "The game couldn't start. The details are in Desperados/android-log.txt.");
            break;
        }
        Uint32 now = SDL_GetTicks();
        if (got > 0) {
            ++frames;
            desp_replay_present(replay, sw, sh, rect);
            SDL_GL_SwapWindow(win);
            if (frames == 1) logf_("first picture from the game after %.1f s", (now - started) / 1000.0);
        } else if (frames == 0) {
            // still loading: keep the screen alive with a moving bar, note progress in the log
            desp_replay_present_waiting(replay, sw, sh, (now - started) / 1000.0f);
            SDL_GL_SwapWindow(win);
            if (now - lastNote >= 15000) {
                lastNote = now;
                unsigned long long bytes = 0, msgs = 0;
                desp_replay_stats(replay, &bytes, &msgs);
                logf_("still waiting for the game's first picture (%u s; %llu commands, %.1f MB from the game so far)",
                      (now - started) / 1000, msgs, bytes / 1048576.0);
                logGameThreads(gamePid);
            }
            if (now - started > 300000) {
                say("Desperados", "The game hasn't shown anything for 5 minutes. The details are in Desperados/android-log.txt.");
                break;
            }
        }
    }
    return leave(0);
}
