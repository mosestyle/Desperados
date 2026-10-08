// Preloaded into the game process (glibc side). Android runs every app process under a seccomp
// filter that answers a system call it doesn't allow with SIGSYS, which kills the process.
// glibc and Box64 cope with system calls that simply don't exist, so this turns each refused
// call into an ENOSYS error instead, and notes it in the log once per call number.
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

static volatile uint8_t reported[512];

static void put(char* buf, size_t* n, const char* s) {
    while (*s && *n < 120) buf[(*n)++] = *s++;
}

static void onSigsys(int sig, siginfo_t* info, void* ctx) {
    (void)sig;
    ucontext_t* uc = (ucontext_t*)ctx;
    int nr = info->si_syscall;
#if defined(__aarch64__)
    uc->uc_mcontext.regs[0] = (uint64_t)(int64_t)-ENOSYS;
#endif
    if (nr >= 0 && nr < 512 && !reported[nr]) {
        reported[nr] = 1;
        char buf[128];
        size_t n = 0;
        put(buf, &n, "[game] Android refused system call ");
        char num[12];
        int k = 0, v = nr;
        do { num[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 11);
        while (k) buf[n++] = num[--k];
        put(buf, &n, ", continuing without it\n");
        ssize_t w = write(2, buf, n);
        (void)w;
    }
}

__attribute__((constructor)) static void installSigsysGuard(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = onSigsys;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSYS, &sa, NULL);
}
