// probes/i14_pipe2.c
//
// 【测试目的】验证 pipe2(O_CLOEXEC|O_NONBLOCK) 可用性。
// pipe2 是 pipe 的增强版，原子性地设置 close-on-exec 和 non-blocking，
// 避免 fork/exec 竞态条件下 fd 泄漏。
//
// 【关联项目】Bun / Playwright / vite-plus / NodeJS
//
// 【上游调用链】
// Bun.spawn() & Playwright 浏览器 stdio 管道:
//   → pipe2(fds, O_CLOEXEC) — 创建浏览器 stdio 管道
//   → Bun spawn_process.rs — pipe2() 用于子进程 stdin/stdout/stderr
//
// Playwright 浏览器进程 stdio:
//   → processLauncher.ts — spawn(browserPath, { stdio: 'pipe' })
//     → Node.js child_process → libuv uv__make_pipe() → pipe2(O_CLOEXEC)
//
// 【影响】pipe2 原子性地创建带 O_CLOEXEC 的管道，防止子进程继承不需要的 fd。
// 若不可用需降级为 pipe() + fcntl(F_SETFD, FD_CLOEXEC)，存在竞态窗口。
//
// Exit: 0=pass, 1=fail

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    int p[2];
    if (pipe2(p, O_CLOEXEC) < 0) {
        fprintf(stderr, "pipe2(O_CLOEXEC) failed: %s", strerror(errno));
        return 1;
    }

    // Verify it works: write and read
    const char *msg = "probe";
    if (write(p[1], msg, 5) != 5) {
        fprintf(stderr, "pipe2 write failed: %s", strerror(errno));
        close(p[0]); close(p[1]);
        return 1;
    }
    char buf[5];
    if (read(p[0], buf, 5) != 5) {
        fprintf(stderr, "pipe2 read failed: %s", strerror(errno));
        close(p[0]); close(p[1]);
        return 1;
    }
    close(p[0]); close(p[1]);
    return 0;
}
