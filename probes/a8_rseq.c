// probes/a8_rseq.c
//
// 【测试目的】验证 rseq (restartable sequences, syscall 293) 可用性。
// rseq 是 Linux 5.1+ 的 per-CPU 无锁原子操作机制，glibc ≥2.35 在
// 线程创建时自动注册 rseq。musl 不注册 rseq（Bun 不受影响）。
//
// 【上游调用链】Bun（运行时验证发现）
//   glibc 线程启动 → __init_tp / start_thread 自动注册 rseq
//     → syscall(SYS_rseq=293, &rseq, sizeof(rseq), 0, RSEQ_SIG)
//       → seccomp filter → SIGSYS → 进程终止
//
// 【影响】仅影响 glibc 链接的运行时（Node.js/Deno），Bun 使用 musl libc
// 不受影响。但 HarmonyOS 上任何 glibc 程序启动即触发 SIGSYS 崩溃。
//
// Exit: 0=pass, 1=fail (SIGSYS)

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/rseq.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    struct rseq rseq_abi = {0};
    long ret = syscall(293, &rseq_abi, sizeof(rseq_abi), 0, 0x53053053 /* RSEQ_SIG */);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked rseq (SIGSYS)");
        return 1;
    }
    if (ret < 0 && errno == ENOSYS) {
        fprintf(stderr, "rseq not implemented");
        return 1;
    }
    return 0;
}
