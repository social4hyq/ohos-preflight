// 替代：musl 启动早期 rseq 注册由 __init_tp 触发，应用层无法事先安装 handler
// 推荐方案 A — 自编译 musl 时屏蔽 rseq 注册（最稳）：
//
//   --- a/src/thread/__init_tp.c
//   +++ b/src/thread/__init_tp.c
//   @@ rseq registration
//   -    int r = __syscall(SYS_rseq, &td->rseq, sizeof td->rseq, 0, RSEQ_SIG);
//   -    if (r) return r;
//   +    /* HarmonyOS seccomp 拦截 rseq → 静默忽略，丢失 per-cpu 优化但保命 */
//   +    (void)__syscall(SYS_rseq, &td->rseq, sizeof td->rseq, 0, RSEQ_SIG);
//
// 推荐方案 B — 应用层 SIGSYS 拦截（仅对 main 之后的 rseq 调用有效）：
#define _GNU_SOURCE
#include <signal.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <errno.h>

static void sigsys_to_enosys(int sig, siginfo_t *info, void *ctx) {
    if (info->si_syscall == 293 /* SYS_rseq */) {
        // aarch64: 让 syscall 返回 -ENOSYS，避免进程被 kill
        ((ucontext_t *)ctx)->uc_mcontext.regs[0] = -ENOSYS;
        return;
    }
    signal(sig, SIG_DFL); raise(sig);
}

__attribute__((constructor(101))) static void early_rseq_shield(void) {
    struct sigaction sa = { .sa_sigaction = sigsys_to_enosys,
                            .sa_flags = SA_SIGINFO | SA_NODEFER };
    sigaction(SIGSYS, &sa, NULL);
}
// TODO: 待 HarmonyOS 放行 rseq (293) 后移除 musl 补丁与 SIGSYS handler
// 注意：方案 B 无法覆盖 musl 启动时 rseq 调用（早于 ctor 运行），必须配合方案 A
