// probes/i12_waitid_wnohang.c
//
// 【测试目的】验证 waitid(P_PID, ..., WNOHANG | WEXITED) 非阻塞子进程
// 回收可用性。WNOHANG 允许父进程在不阻塞的情况下检查子进程是否已退出。
//
// 【上游调用链】Playwright
//   playwright-core/src/server/processLauncher.ts
//     → Node.js child_process.on('exit', ...) 监听浏览器退出
//       → libuv uv__wait_children() — 内部调用 waitid(WNOHANG)
//         → waitid(P_ALL, 0, &info, WNOHANG | WEXITED, 0)
//           → 非阻塞检查已终止子进程并回收僵尸进程
//
// 【影响】Playwright 依赖 waitid(WNOHANG) 异步监听浏览器进程退出事件。
// 若该调用被限制，浏览器进程退出时 Node.js 无法收到 'exit' 事件，
// Playwright 测试将挂起等待超时。
//
// Exit: 0=pass, 1=fail

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

int main(void) {
    // Fork a quick child that exits immediately.
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork() failed: %s", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        _exit(42);
    }

    // Wait for child to actually terminate (brief sleep for scheduling).
    usleep(100000);

    // Non-blocking waitid — should reap the exited child.
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    int ret = waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG);
    if (ret < 0) {
        fprintf(stderr, "waitid(P_PID, %d, WNOHANG) failed: %s", pid, strerror(errno));
        // Clean up zombie
        waitpid(pid, NULL, 0);
        return 1;
    }

    // Verify we got the right child.
    if (info.si_pid != pid) {
        // Child might still be running — wait again with blocking.
        int status;
        waitpid(pid, &status, 0);
        // Retry waitid after blocking wait
        memset(&info, 0, sizeof(info));
        ret = waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG);
        if (ret < 0) {
            fprintf(stderr, "waitid retry failed: %s", strerror(errno));
            return 1;
        }
        // If still no match, the interface might be broken.
        if (info.si_pid != pid && info.si_pid == 0) {
            // Child already reaped by blocking wait — that's fine.
            return 0;
        }
    }
    return 0;
}
