// probes/a9_pidfd_open.c
//
// 【测试目的】pidfd_open (434) 进程 fd — Bun add spawn 子进程。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun.spawn() 子进程生命周期追踪:
//   → linux_syscall::pidfd_open(pid, flags)                   bun/src/sys/linux_syscall.rs:481
//     → libc::syscall(434, pid, flags)                        硬编码 SYS_pidfd_open (Android cfg)
//       → seccomp filter → SIGSYS
// 
// WebKit Syscalls.h 已定义预留:
//   → __NR_pidfd_open (434)                                     Syscalls.h — 预留未来使用
//
// 【影响】Bun.spawn() 子进程生命周期追踪依赖 pidfd_open。调用被 seccomp 拦截（SIGSYS），影响 Bun 的进程管理和子进程退出检测。
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

    // Bun: libc::syscall(434, pid, flags) — hardcoded on Android
    long ret = syscall(434, getpid(), 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked pidfd_open (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "pidfd_open not implemented");
        return 1;
    }
    if (ret >= 0) close((int)ret);
    return 0;
}

