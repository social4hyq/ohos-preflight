// probes/c15_futex.c
//
// 【测试目的】futex 快速用户锁 — pthread 同步原语基础。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun / pthread 同步原语 (mutex/semaphore/condvar):
//   → libc::syscall(SYS_futex, uaddr, op, val,                 libc crate (pthread 内部)
//       timeout, uaddr2, val3)
//     → 内核 futex(2)
// 
// WebKit 内部直接 futex 调用:
//   → syscall(SYS_futex, ...)                                   ANGLE SimpleMutex.cpp:36
//   → syscall(SYS_futex, ...)                                   libwebrtc futex.h:130,140
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdatomic.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

#define FUTEX_WAKE 1

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    _Atomic int futex_word = 0;
    // Attempt a wake; 0 waiters → returns 0 (syscall exists)
    long ret = syscall(SYS_futex, &futex_word, FUTEX_WAKE, 1, 0, 0, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked futex (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "futex not implemented");
        return 1;
    }
    return 0;
}

