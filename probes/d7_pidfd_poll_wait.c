// probes/d7_pidfd_poll_wait.c
//
// 【测试目的】验证 pidfd_open + poll + wait4 组合（fork 替代 wait_linux_signalfd 的方案）
//             — fork spawn/process.rs no_orphans 分支依赖此组合。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.spawn() no_orphans 父死检测:
//   → spawn/process.rs:3252 OHOS 分支
//     → pidfd_open(ppid, 0)
//     → poll([pidfd, stdio_fds], timeout)
//     → wait4(pid, ..., WNOHANG)
//
// 【为何需要】fork 注释说 wait_linux_signalfd 在 OHOS 上 hang（signalfd+pidfd 组合卡死），
// 替代方案是 poll(pidfd) 检测父死 + wait4(child) 回收。
// 本探针端到端验证替代方案可用。
//
// 【影响】若 fail：OHOS 上 no_orphans 模式不可用，必须禁用或彻底重写；
//         若 pass：fork 的 poll+wait4 替代是正确路径。
//
// Exit: 0=pass (pidfd_open + poll(POLLIN) + wait4 全链可用), 1=fail

#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

static int pidfd_open(pid_t pid, unsigned int flags) {
    return (int)syscall(SYS_pidfd_open, pid, flags);
}

int main(void) {
    pid_t child = fork();
    if (child < 0) {
        fprintf(stderr, "fork: %s", strerror(errno));
        return 1;
    }
    if (child == 0) {
        usleep(150 * 1000);
        _exit(0);
    }

    int pfd = pidfd_open(child, 0);
    if (pfd < 0) {
        fprintf(stderr, "pidfd_open(%d): %s", child, strerror(errno));
        waitpid(child, NULL, 0);
        return 1;
    }

    struct pollfd pf = { .fd = pfd, .events = POLLIN };
    int pr = poll(&pf, 1, 3000);
    if (pr < 0) {
        fprintf(stderr, "poll(pidfd): %s", strerror(errno));
        close(pfd);
        waitpid(child, NULL, 0);
        return 1;
    }
    if (pr == 0) {
        fprintf(stderr, "poll(pidfd) timed out — OHOS quirk: pidfd readiness not delivered");
        close(pfd);
        waitpid(child, NULL, 0);
        return 1;
    }
    if (!(pf.revents & POLLIN)) {
        fprintf(stderr, "poll returned revents=0x%x (expected POLLIN)", pf.revents);
        close(pfd);
        waitpid(child, NULL, 0);
        return 1;
    }

    int status = 0;
    pid_t r = wait4(child, &status, WNOHANG, NULL);
    if (r <= 0) {
        fprintf(stderr, "wait4(WNOHANG) after pidfd POLLIN returned %d: %s", r, strerror(errno));
        close(pfd);
        return 1;
    }
    close(pfd);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "child exited abnormally: status=%d", status);
        return 1;
    }
    return 0;
}
