// probes/c13_epoll_pwait2.c
//
// 【测试目的】验证 epoll_pwait2 (syscall 441) 可用性。
// epoll_pwait2 提供纳秒级超时精度和信号掩码，是 epoll_pwait 的增强版
//（需要 Linux ≥5.11）。
//
// 【上游调用链】Bun
//   sys_epoll_pwait2(epfd, events, max, timeout, sigmask)    platform/linux.rs:29
//     → libc::syscall(SYS_epoll_pwait2=441, epfd, events,
//         max, timeout, sigmask, 8)                           内联汇编 raw syscall
//       → seccomp filter → SIGSYS
//
// 【影响】Bun 事件循环依赖 epoll_pwait2 实现纳秒级超时等待。
// HarmonyOS seccomp 拦截该调用（SIGSYS），Bun 无法启动事件循环。
// 降级方案：epoll_pwait + timerfd 补齐纳秒级唤醒（j5_epoll_pwait2_fb）。
//
// Exit: 0=pass, 1=fail (SIGSYS/ENOSYS)

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/epoll.h>
#include <fcntl.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event events[1];
    struct timespec timeout = {0, 0};
    long ret = syscall(SYS_epoll_pwait2, epfd, events, 1,
                       &timeout, NULL, (size_t)8);
    if (sigsys_caught) {
        close(epfd);
        fprintf(stderr, "seccomp blocked epoll_pwait2 (SIGSYS)");
        return 1;
    }
    close(epfd);
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "epoll_pwait2 not implemented");
        return 1;
    }
    return 0;
}
