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
#include <android/log.h>
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
        if (r <= 0) break;
        if (SDL_GetQueuedAudioSize(audioDev) > bytesPerSec / 4) continue;  // keep the delay short
        SDL_QueueAudio(audioDev, buf, (Uint32)r);
    }
    close(fd);
    return 0;
}

// ---------------------------------------------------------------------------------------------

static pid_t startGame(const char* game, const char* libDir, const char* linkDir, const char* x86Dir,
                       const char* fifo, const char* userDir, const char* rcFile, const char* runDir,
                       int sock, int logFd, int gw, int gh) {
    char ld[600], box[600], exe[600], ldPath[1200];
    snprintf(ld, sizeof ld, "%s/libglibc_ld.so", libDir);
    snprintf(box, sizeof box, "%s/libbox64g.so", libDir);
    snprintf(exe, sizeof exe, "%s/desperados32", game);
    snprintf(ldPath, sizeof ldPath, "%s", linkDir);
    char e[24][700];
    int n = 0;
    snprintf(e[n++], 700, "LD_LIBRARY_PATH=%s", linkDir);
    snprintf(e[n++], 700, "LD_PRELOAD=%s/libchild_sigsys.so", libDir);  // see child/sigsys.c
    snprintf(e[n++], 700, "BOX64_LD_LIBRARY_PATH=%s", x86Dir);
    snprintf(e[n++], 700, "BOX64_LOG=1");
    snprintf(e[n++], 700, "BOX64_SHOWSEGV=1");
    // Box64 needs a settings file: without one it falls back to a shared-memory file, which
    // Android doesn't have, and then crashes while starting (seen as a crash right after it
    // prints the CPU info)
    snprintf(e[n++], 700, "BOX64_RCFILE=%s", rcFile);
    snprintf(e[n++], 700, "BOX64_NOBANNER=0");
    snprintf(e[n++], 700, "DESP_BRIDGE_FD=%d", sock);
    snprintf(e[n++], 700, "DESP_GAME_EXE=%s", exe);
    snprintf(e[n++], 700, "SDL_VIDEODRIVER=offscreen");
    snprintf(e[n++], 700, "SDL_AUDIODRIVER=disk");
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
    char* argv[] = {ld, "--library-path", ldPath, box, exe, NULL};

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

int SDL_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    signal(SIGPIPE, SIG_IGN);
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) return 1;

    // the game folder
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
            "as a folder named 'Desperados', allow this app to access all files, then start it again.");
        return 0;
    }
    snprintf(path, sizeof path, "%s/android-log.txt", game);
    logFile = fopen(path, "w");
    int logFd = logFile ? fileno(logFile) : -1;
    logf_("Desperados Original: game folder %s", game);

    // window and GLES 3 context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_Window* win = SDL_CreateWindow("Desperados", 0, 0, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!win) { say("Desperados", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { say("Desperados", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    int sw, sh;
    SDL_GL_GetDrawableSize(win, &sw, &sh);
    // the game draws at the phone's aspect ratio, 768 pixels high
    int gh = 768, gw = sw > 0 && sh > 0 ? (int)((long)768 * sw / sh) & ~1 : 1024;
    if (gw < 1024) { gw = 1024; gh = sh > 0 ? (int)((long)1024 * sh / sw) & ~1 : 768; }
    logf_("screen %dx%d, game window %dx%d", sw, sh, gw, gh);

    // runtime files
    char libDir[512], files[512], linkDir[600], x86Dir[600], fifo[600], userDir[600], rcFile[600];
    nativeLibDir(libDir, sizeof libDir);
    snprintf(files, sizeof files, "%s", SDL_AndroidGetInternalStoragePath());
    snprintf(linkDir, sizeof linkDir, "%s/rt", files);
    snprintf(x86Dir, sizeof x86Dir, "%s/x86lib", files);
    snprintf(fifo, sizeof fifo, "%s/audio.pipe", files);
    snprintf(userDir, sizeof userDir, "%s/userdata", game);
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
    snprintf(rcFile, sizeof rcFile, "%s/box64.box64rc", files);
    {
        FILE* f = fopen(rcFile, "w");
        if (f) {
            fputs("# Box64 settings for the game process\n[desperados32]\nBOX64_DYNAREC_BIGBLOCK=1\n", f);
            fclose(f);
        } else logf_("can't write %s: %s", rcFile, strerror(errno));
    }
    unlink(fifo);
    if (mkfifo(fifo, 0600) != 0) logf_("mkfifo: %s", strerror(errno));

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { say("Desperados", "socketpair failed"); return 1; }
    logf_("starting the game process (libraries in %s)", libDir);
    pid_t pid = startGame(game, libDir, linkDir, x86Dir, fifo, userDir, rcFile, files, sv[1], logFd, gw, gh);
    close(sv[1]);
    if (pid < 0) { say("Desperados", "Couldn't start the game process."); return 1; }
    SDL_CreateThread(audioThread, "audio", fifo);

    desp_replay* r = desp_replay_create(sv[0]);
    int rect[4] = {0, 0, sw, sh};
    touch_t t = {0};
    int frames = 0, running = 1;
    Uint32 lastFrame = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_KEYDOWN: case SDL_KEYUP:
                if (ev.key.keysym.sym == SDLK_AC_BACK) sendKey(r, ev.type == SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
                else sendKey(r, ev.type == SDL_KEYDOWN, ev.key.keysym.scancode, ev.key.keysym.sym);
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) SDL_GL_GetDrawableSize(win, &sw, &sh);
                break;
            case SDL_FINGERDOWN:
                if (t.active) break;  // one finger at a time for now
                t.active = 1;
                t.id = ev.tfinger.fingerId;
                t.dragged = t.longFired = 0;
                t.sx = ev.tfinger.x * sw;
                t.sy = ev.tfinger.y * sh;
                t.downAt = ev.tfinger.timestamp;
                {
                    int gx, gy;
                    toGame(r, rect, sh, t.sx, t.sy, &gx, &gy);
                    sendMouse(r, IN_MOUSE_TO, gx, gy, 0);
                }
                break;
            case SDL_FINGERMOTION:
                if (!t.active || ev.tfinger.fingerId != t.id) break;
                {
                    float x = ev.tfinger.x * sw, y = ev.tfinger.y * sh;
                    float dx = x - t.sx, dy = y - t.sy, slop = sh * 0.03f;
                    if (dx * dx + dy * dy > slop * slop) t.dragged = 1;
                    if (t.dragged) {
                        int gx, gy;
                        toGame(r, rect, sh, x, y, &gx, &gy);
                        sendMouse(r, IN_MOUSE_TO, gx, gy, 0);
                    }
                }
                break;
            case SDL_FINGERUP:
                if (!t.active || ev.tfinger.fingerId != t.id) break;
                t.active = 0;
                if (!t.dragged && !t.longFired) {
                    int gx, gy;
                    toGame(r, rect, sh, t.sx, t.sy, &gx, &gy);
                    sendMouse(r, IN_MOUSE_TO, gx, gy, 0);
                    sendMouse(r, IN_MOUSE_DOWN, gx, gy, SDL_BUTTON_LEFT);
                    sendMouse(r, IN_MOUSE_UP, gx, gy, SDL_BUTTON_LEFT);
                }
                break;
            default: break;
            }
        }
        // long press: right click
        if (t.active && !t.dragged && !t.longFired && SDL_GetTicks() - t.downAt > 500) {
            int gx, gy;
            toGame(r, rect, sh, t.sx, t.sy, &gx, &gy);
            sendMouse(r, IN_MOUSE_TO, gx, gy, 0);
            sendMouse(r, IN_MOUSE_DOWN, gx, gy, SDL_BUTTON_RIGHT);
            sendMouse(r, IN_MOUSE_UP, gx, gy, SDL_BUTTON_RIGHT);
            t.longFired = 1;
        }

        int got = desp_replay_frame(r, 8);
        if (got == 0) {
            int status = 0;
            for (int i = 0; i < 20 && waitpid(pid, &status, WNOHANG) == 0; ++i) SDL_Delay(50);
            if (WIFSIGNALED(status))
                logf_("the game process was stopped by signal %d (%s) after %d frames", WTERMSIG(status),
                      WTERMSIG(status) == SIGSYS ? "a system call Android doesn't allow" : strsignal(WTERMSIG(status)), frames);
            else
                logf_("the game process ended (exit code %d, status %#x) after %d frames", WEXITSTATUS(status), status, frames);
            say("Desperados", frames ? "The game has ended." :
                "The game couldn't start. The details are in Desperados/android-log.txt.");
            break;
        }
        if (got > 0) {
            ++frames;
            lastFrame = SDL_GetTicks();
            desp_replay_present(r, sw, sh, rect);
            SDL_GL_SwapWindow(win);
            if (frames == 1) logf_("first frame from the game");
        } else if (frames == 0 && SDL_GetTicks() - lastFrame > 120000) {
            say("Desperados", "The game hasn't shown anything for 2 minutes. The details are in Desperados/android-log.txt.");
            break;
        }
    }
    kill(pid, SIGTERM);
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    desp_replay_destroy(r);
    if (logFile) fclose(logFile);
    exit(0);
}
