// probes/a10_close_range.c
//
// 【测试目的】验证 close_range (syscall 436) 可用性。
// close_range 批量关闭指定范围内的文件描述符，比逐个 close() 快 100-1000 倍。
//
// 【上游调用链】Bun
//   Bun.spawn() 子进程 fd 清理
//     → POSIX_SPAWN_CLOEXEC_DEFAULT (0x4000)           spawn_process.rs:578
//       → fcntl(F_SETFD, FD_CLOEXEC) per fd              spawn_process.rs:612
//         → libc::syscall(SYS_close_range=436, ...)       linux_syscall.rs
//           → seccomp filter → SIGSYS
//
// 【影响】Bun.spawn() 创建子进程后调用 close_range 关闭继承的 fd。
// HarmonyOS seccomp 拦截该调用（SIGSYS），子进程 fd 泄漏或异常退出。
// 降级方案：通过 /proc/self/fd 枚举实际打开的 fd 逐个关闭（j2_close_range_fb）。
//
// Exit: 0=pass, 1=fail (SIGSYS/ENOSYS)

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    long ret = syscall(SYS_close_range, ~0U, 0, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked close_range (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "close_range not implemented");
        return 1;
    }
    return 0;
}
