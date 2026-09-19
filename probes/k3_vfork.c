// probes/k3_vfork.c
//
// 【测试目的】验证 vfork() 本身是否可用（不是 fork()）。ohos-bun 的
// posix_spawn_bun() 在 `#if OS(LINUX) && !defined(__OHOS__)` 分支才会
// 尝试 vfork()，OHOS 分支被硬编码跳过，从未在真机上实测过 vfork() 到底
// 会不会被拦 —— 这是一条纯粹基于谨慎假设、从未验证过的降级。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// Bun.spawn() / Bun.spawnSync():
//   → posix_spawn_bun()                                  jsc/bindings/bun-spawn.cpp:168
//     → #if OS(LINUX) && !defined(__OHOS__): vfork()，失败才回退 fork()
//     → OHOS 分支：直接 fork()（更慢，需要额外的自管道错误检测）
//
// 【为何需要】aarch64 没有专用 vfork 系统调用号，musl 的 vfork() 走
// clone(CLONE_VM|CLONE_VFORK|SIGCHLD, ...) —— 这组 flags 和普通 fork()
// 用的 clone() 不同，seccomp 完全可能只放行后者。本探针直接测前者。
//
// Exit: 0=pass (vfork 成功且子进程正常退出 — OHOS 分支的强制 fork 降级可以撤销)
//       1=fail (SIGSYS/EAGAIN 等 — 强制 fork 仍然必需)

#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    pid_t pid = vfork();
    if (pid == 0) {
        // vfork() child: must not touch parent's stack/heap state beyond
        // this point. _exit (not exit) skips atexit/stdio flushing, which
        // would corrupt the still-shared parent memory.
        _exit(0);
    }

    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked vfork (SIGSYS)");
        return 1;
    }
    if (pid < 0) {
        fprintf(stderr, "vfork: %s", strerror(errno));
        return 1;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s", strerror(errno));
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "vfork child exited abnormally (raw status %d)", status);
        return 1;
    }
    return 0;
}
