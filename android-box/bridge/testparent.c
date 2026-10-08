// Desktop test harness for the bridge: plays the role of the Android app. Starts the game
// process (argv[1..]) with the bridge's libGL, replays its frames in an SDL window (OpenGL ES 3),
// and follows a little script of inputs and screenshots:
//   HARNESS_SCRIPT="120:shot;150:click 297 326;400:shot;900:quit"   (frame:action)
//   click / move / rclick take game window coordinates.
#include <SDL.h>
#include <GLES3/gl3.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "replay.h"

static void shot(desp_replay* r, int frame) {
    int w, h;
    desp_replay_game_size(r, &w, &h);
    if (w <= 0) return;
    unsigned char* px = malloc((size_t)w * h * 4);
    if (!desp_replay_read_frame(r, px)) { free(px); return; }
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
    for (int y = 0; y < h; ++y) memcpy((uint8_t*)s->pixels + (size_t)y * s->pitch, px + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
    char name[64];
    snprintf(name, sizeof name, "shot_%05d.bmp", frame);
    SDL_SaveBMP(s, name);
    SDL_FreeSurface(s);
    free(px);
    fprintf(stderr, "[harness] saved %s\n", name);
}

static int curX = 512, curY = 384;
static void mouse(desp_replay* r, int type, int x, int y, int button) {
    desp_input in = {0};
    if (type == IN_MOUSE_MOTION) {  // the game moves its own cursor by relative motion: pin it to the corner first
        in.type = IN_MOUSE_MOTION;
        in.dx = -10000;
        in.dy = -10000;
        desp_replay_send_input(r, &in);
        curX = curY = 0;
    }
    in.type = (uint32_t)type;
    in.x = x;
    in.y = y;
    in.dx = x - curX;
    in.dy = y - curY;
    in.button = (uint32_t)button;
    curX = x;
    curY = y;
    desp_replay_send_input(r, &in);
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: testparent game [args]\n"); return 1; }
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { perror("socketpair"); return 1; }
    pid_t pid = fork();
    if (pid == 0) {
        close(sv[0]);
        char fd[16];
        snprintf(fd, sizeof fd, "%d", sv[1]);
        setenv("DESP_BRIDGE_FD", fd, 1);
        setenv("SDL_VIDEODRIVER", "offscreen", 1);
        execvp(argv[1], argv + 1);
        perror("exec");
        _exit(127);
    }
    close(sv[1]);
    signal(SIGPIPE, SIG_IGN);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_Window* win = SDL_CreateWindow("bridge test", 0, 0, 1280, 720, SDL_WINDOW_OPENGL);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { fprintf(stderr, "no GLES context: %s\n", SDL_GetError()); return 1; }
    fprintf(stderr, "[harness] %s\n", glGetString(GL_VERSION));

    desp_replay* r = desp_replay_create(sv[0]);
    const char* script = getenv("HARNESS_SCRIPT");
    int frame = 0;
    for (;;) {
        int got = desp_replay_frame(r, 30000);
        if (got == 0) { fprintf(stderr, "[harness] game process ended after %d frames\n", frame); break; }
        if (got < 0) { fprintf(stderr, "[harness] no frame for 30 s (frame %d)\n", frame); break; }
        ++frame;
        desp_replay_present(r, 1280, 720, NULL);
        SDL_GL_SwapWindow(win);
        if (frame % 100 == 0) fprintf(stderr, "[harness] frame %d\n", frame);
        // script actions for this frame
        for (const char* s = script; s && *s;) {
            int f = atoi(s);
            const char* colon = strchr(s, ':');
            const char* next = strchr(s, ';');
            if (colon && f == frame) {
                char act[64] = {0};
                size_t n = (size_t)((next ? next : s + strlen(s)) - colon - 1);
                memcpy(act, colon + 1, n < sizeof act - 1 ? n : sizeof act - 1);
                int x = 0, y = 0;
                if (!strcmp(act, "shot")) shot(r, frame);
                else if (!strcmp(act, "quit")) { kill(pid, SIGTERM); goto done; }
                else if (sscanf(act, "move %d %d", &x, &y) == 2) mouse(r, IN_MOUSE_MOTION, x, y, 0);
                else if (sscanf(act, "click %d %d", &x, &y) == 2) {
                    if (x != curX || y != curY) mouse(r, IN_MOUSE_MOTION, x, y, 0);
                    mouse(r, IN_MOUSE_DOWN, x, y, 1);
                    mouse(r, IN_MOUSE_UP, x, y, 1);
                } else if (sscanf(act, "tap %d %d", &x, &y) == 2) {  // window pixels, converted by the game side
                    desp_input in = {0};
                    in.type = IN_MOUSE_TO; in.x = x; in.y = y;
                    desp_replay_send_input(r, &in);
                    mouse(r, IN_MOUSE_DOWN, x, y, 1);
                    mouse(r, IN_MOUSE_UP, x, y, 1);
                } else if (sscanf(act, "to %d %d", &x, &y) == 2) {  // just put the cursor there
                    desp_input in = {0};
                    in.type = IN_MOUSE_TO; in.x = x; in.y = y;
                    desp_replay_send_input(r, &in);
                } else if (sscanf(act, "rclick %d %d", &x, &y) == 2) {
                    mouse(r, IN_MOUSE_MOTION, x, y, 0);
                    mouse(r, IN_MOUSE_DOWN, x, y, 3);
                    mouse(r, IN_MOUSE_UP, x, y, 3);
                }
            }
            s = next ? next + 1 : NULL;
        }
    }
done:
    waitpid(pid, NULL, WNOHANG);
    desp_replay_destroy(r);
    SDL_Quit();
    return 0;
}
