// probes/c9_getrandom.c
//
// 【测试目的】getrandom 内核随机数 — Bun workaround-missing-symbols.cpp:173。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun crypto.randomUUID() / crypto.getRandomValues()
//   → getrandom::getrandom(buf)                            getrandom crate v0.4 (Cargo.toml)
//     → libc::getrandom(buf, len, 0)                       libc crate
//       → 内核 getrandom(2)
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    // GRND_NONBLOCK=1: don't block even if no entropy yet
    unsigned char buf[16];
    long ret = syscall(SYS_getrandom, buf, sizeof(buf), 1U);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked getrandom (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "getrandom not implemented");
        return 1;
    }
    return 0;
}

