// 替代：进程内自捕获崩溃信号 + backtrace，配合 HiAppEvent 上报
// 适用场景：真机崩溃排障；无法替代 step / breakpoint 等交互式调试
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static void crash_handler(int sig, siginfo_t *si, void *ctx) {
    void *frames[64];
    int n = backtrace(frames, 64);
    dprintf(STDERR_FILENO, "[crash] signal=%d addr=%p frames=%d\n",
            sig, si->si_addr, n);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    // 可在此调用 HiAppEvent_Write 上报到 DevEco
    _exit(128 + sig);
}

__attribute__((constructor)) static void install_crash_handler(void) {
    struct sigaction sa = { .sa_sigaction = crash_handler,
                            .sa_flags = SA_SIGINFO };
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
}
// TODO: 待 HarmonyOS 放行 ptrace(PTRACE_TRACEME) 后可直接用 gdb/lldb attach
