// libmain.so: starts the original Linux build of Desperados (desperados32) inside this app.
//
// SDL calls SDL_main() on its own thread once the Android window exists. From here:
//   1. find the game folder the user copied to the phone (/storage/emulated/0/Desperados),
//   2. copy the game program and the x86 runtime libraries (libstdc++, libgcc_s, from the APK)
//      to the app's private storage (shared storage can't hold program code),
//   3. send everything the game prints to Desperados/android-log.txt for troubleshooting,
//   4. run the game through Box64 (Box32 mode: 32-bit x86 on the phone's 64-bit ARM CPU),
//      loaded as a library into this same process, so the game's SDL2 calls reach this app's
//      SDL2: its window, touch screen and sound.
#include <SDL.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "Desperados", __VA_ARGS__)

extern char** environ;

static void say(const char* title, const char* text) {
    LOG("%s: %s", title, text);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, text, NULL);
}

static int exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Copies a file (an APK asset when `src` is relative) unless an identical-size copy is there.
static int copyFile(const char* src, const char* dst, int executable) {
    SDL_RWops* in = SDL_RWFromFile(src, "rb");
    if (!in) { LOG("cannot open %s: %s", src, SDL_GetError()); return 0; }
    Sint64 size = SDL_RWsize(in);
    struct stat st;
    if (stat(dst, &st) == 0 && st.st_size == size) { SDL_RWclose(in); return 1; }
    FILE* out = fopen(dst, "wb");
    if (!out) { LOG("cannot write %s: %s", dst, strerror(errno)); SDL_RWclose(in); return 0; }
    char buf[65536];
    size_t n;
    while ((n = SDL_RWread(in, buf, 1, sizeof buf)) > 0) fwrite(buf, 1, n, out);
    fclose(out);
    SDL_RWclose(in);
    chmod(dst, executable ? 0755 : 0644);
    return 1;
}

int SDL_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
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

    // log file next to the game, for troubleshooting
    char logPath[1024];
    snprintf(logPath, sizeof logPath, "%s/android-log.txt", game);
    if (freopen(logPath, "w", stdout)) {
        dup2(fileno(stdout), fileno(stderr));
        setvbuf(stdout, NULL, _IOLBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    printf("Desperados for Android: game folder %s\n", game);

    // program code goes to private storage
    const char* priv = SDL_AndroidGetInternalStoragePath();
    char libDir[1024], exe[1024];
    snprintf(libDir, sizeof libDir, "%s/x86lib", priv);
    mkdir(libDir, 0755);
    snprintf(exe, sizeof exe, "%s/desperados32", priv);
    snprintf(path, sizeof path, "%s/desperados32", game);
    int ok = copyFile(path, exe, 1);
    static const char* libs[] = {"libstdc++.so.6", "libgcc_s.so.1", NULL};
    for (int i = 0; libs[i]; ++i) {
        char src[256], dst[1024];
        snprintf(src, sizeof src, "x86lib/%s", libs[i]);
        snprintf(dst, sizeof dst, "%s/%s", libDir, libs[i]);
        ok &= copyFile(src, dst, 0);
    }
    if (!ok) { say("Desperados", "Couldn't prepare the game files (see android-log.txt)."); return 0; }

    // Box64 settings
    setenv("BOX64_LD_LIBRARY_PATH", libDir, 1);
    setenv("BOX64_LIBGL", "libGLdesp.so", 1);
    setenv("BOX64_LOG", "1", 0);
    setenv("BOX64_SHOWSEGV", "1", 0);
    setenv("BOX64_DYNAREC_BIGBLOCK", "1", 0);
    setenv("BOX64_NORCFILES", "1", 1);
    setenv("HOME", priv, 1);
    setenv("XDG_DATA_HOME", priv, 1);
    if (chdir(game) != 0) { say("Desperados", "Couldn't open the game folder."); return 0; }

    void* box = dlopen("libbox64.so", RTLD_NOW | RTLD_GLOBAL);
    if (!box) {
        printf("dlopen libbox64.so failed: %s\n", dlerror());
        say("Desperados", "Couldn't load the translator (libbox64.so). See android-log.txt.");
        return 0;
    }
    int (*box64_main)(int, const char**, char**) = (int (*)(int, const char**, char**))dlsym(box, "box64_main");
    if (!box64_main) { say("Desperados", "libbox64.so has no box64_main."); return 0; }

    const char* args[] = {"box64", exe, NULL};
    printf("starting %s\n", exe);
    fflush(stdout);
    int rc = box64_main(2, args, environ);
    printf("the game ended (%d)\n", rc);
    fflush(stdout);
    exit(rc);  // Box64 leaves threads and signal handlers behind: end the process cleanly
}
