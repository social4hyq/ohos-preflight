// probes/c14_gettid.c
//
// 【测试目的】gettid 线程 ID — pthread/musl 内部调用。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun / pthread 获取线程 ID:
//   → libc::syscall(SYS_gettid)                                libc crate (pthread 内部)
//     → 内核 gettid(2)
// 
// WebKit 线程 ID 检测:
//   → syscall(SYS_gettid)                                      ThreadingPOSIX.cpp:238
//   → getpid() == syscall(SYS_gettid) (musl 主线程检测)        StackBounds.cpp:149
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

    long tid = syscall(SYS_gettid);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked gettid (SIGSYS)");
        return 1;
    }
    if (tid == -1 && errno == ENOSYS) {
        fprintf(stderr, "gettid not implemented");
        return 1;
    }
    return 0;
}

