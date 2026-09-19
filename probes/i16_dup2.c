// probes/i16_dup2.c
//
// 【测试目的】验证 dup2() 文件描述符重定向可用性。
// dup2 用于重定向子进程的 stdin/stdout/stderr，是进程管理的核心机制。
//
// 【关联项目】Bun / Playwright / vite-plus / NodeJS
//
// 【上游调用链】
// Playwright 浏览器 stdio 重定向:
//   → processLauncher.ts — spawn(browserPath, { stdio: 'pipe' })
//     → Node.js child_process → dup2(pipe_fd, STDIN_FILENO)
//     → dup2(pipe_fd, STDOUT_FILENO) / dup2(pipe_fd, STDERR_FILENO)
//
// Bun.spawn() & vite-plus fspy:
//   → Bun spawn_process.rs — dup2() 重定向子进程 stdio
//   → fspy_seccomp_unotify — dup2() 用于虚拟文件系统 fd 替换
//
// 【影响】所有进程 spawn 操作依赖 dup2 重定向 stdin/stdout/stderr。
// 若被限制，子进程 stdio 无法正确建立，浏览器无法启动。
//
// Exit: 0=pass, 1=fail

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    // Test dup2: duplicate STDOUT_FILENO to a temp fd, then write to verify
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) {
        fprintf(stderr, "dup() failed: %s", strerror(errno));
        return 1;
    }

    // Open /dev/null
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd < 0) {
        fprintf(stderr, "open(/dev/null) failed: %s", strerror(errno));
        close(saved);
        return 1;
    }

    // Redirect stdout to /dev/null
    if (dup2(null_fd, STDOUT_FILENO) < 0) {
        fprintf(stderr, "dup2(null_fd, STDOUT_FILENO) failed: %s", strerror(errno));
        close(null_fd); close(saved);
        return 1;
    }

    // Restore stdout
    if (dup2(saved, STDOUT_FILENO) < 0) {
        fprintf(stderr, "dup2(saved, STDOUT_FILENO) failed: %s", strerror(errno));
        close(null_fd); close(saved);
        return 1;
    }

    close(null_fd); close(saved);
    return 0;
}
