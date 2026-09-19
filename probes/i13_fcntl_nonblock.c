// probes/i13_fcntl_nonblock.c
//
// 【测试目的】验证 fcntl(F_GETFL) / fcntl(F_SETFL, O_NONBLOCK) 可用性。
// 设置文件描述符为非阻塞模式是所有异步 I/O 的基础。
//
// 【关联项目】Bun / Playwright / vite-plus / NodeJS
//
// 【上游调用链】
// Bun 事件循环 & Playwright 浏览器 stdio:
//   → fcntl(fd, F_GETFL) — 读取当前 fd 状态标志
//   → fcntl(fd, F_SETFL, flags | O_NONBLOCK) — 设置非阻塞
//   → libuv uv__nonblock() → fcntl(fd, F_SETFL, flags|O_NONBLOCK)
//   → Node.js net.Socket → libuv uv_tcp_open → fcntl(O_NONBLOCK)
//
// Playwright 浏览器 stdio 管道:
//   → processLauncher.ts — spawn(browserPath, { stdio: 'pipe' })
//     → Node.js child_process → libuv uv_pipe_open → fcntl(O_NONBLOCK)
//
// 【影响】所有异步 I/O（事件循环、pipe、socket）依赖 fcntl(O_NONBLOCK)。
// 若被限制，Bun/Playwright/Node.js 全部无法进行非阻塞 I/O。
//
// Exit: 0=pass, 1=fail

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    // Use a pipe as a test fd
    int p[2];
    if (pipe(p) < 0) {
        fprintf(stderr, "pipe() failed: %s", strerror(errno));
        return 1;
    }

    // Test F_GETFL
    int flags = fcntl(p[0], F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "fcntl(F_GETFL) failed: %s", strerror(errno));
        close(p[0]); close(p[1]);
        return 1;
    }

    // Test F_SETFL with O_NONBLOCK
    if (fcntl(p[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        fprintf(stderr, "fcntl(F_SETFL, O_NONBLOCK) failed: %s", strerror(errno));
        close(p[0]); close(p[1]);
        return 1;
    }

    // Verify: read from non-blocking pipe with no data should return EAGAIN
    char buf[1];
    ssize_t n = read(p[0], buf, 1);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "non-blocking read expected EAGAIN, got: %s", strerror(errno));
        close(p[0]); close(p[1]);
        return 1;
    }

    close(p[0]); close(p[1]);
    return 0;
}
