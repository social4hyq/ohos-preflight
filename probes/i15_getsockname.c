// probes/i15_getsockname.c
//
// 【测试目的】验证 getsockname() 可用性。
// getsockname 获取 socket 本地地址，Playwright 用其检测 CDP 连接实际端口。
//
// 【关联项目】Playwright
//
// 【上游调用链】
// Playwright CDP WebSocket 端口检测:
//   → browserType.launch() — 启动浏览器 → 分配随机端口
//     → net.Server.listen(0) → 内核分配可用端口
//       → getsockname(fd, &addr, &len) — 获取实际分配的端口号
//         → Playwright 读取端口 → 构造 CDP WebSocket URL
//
// 【影响】Playwright 启动浏览器时分配随机端口（port 0），通过 getsockname
// 获取实际端口号。若被限制，Playwright 无法获知 CDP 连接端口，浏览器启动失败。
//
// Exit: 0=pass, 1=fail

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket() failed: %s", strerror(errno));
        return 1;
    }

    // Bind to port 0 (kernel assigns a random port)
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = 0,
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind() failed: %s", strerror(errno));
        close(fd);
        return 1;
    }

    // getsockname should return the actual port assigned
    struct sockaddr_in bound;
    socklen_t len = sizeof(bound);
    if (getsockname(fd, (struct sockaddr *)&bound, &len) < 0) {
        fprintf(stderr, "getsockname() failed: %s", strerror(errno));
        close(fd);
        return 1;
    }

    if (ntohs(bound.sin_port) == 0) {
        fprintf(stderr, "getsockname returned port 0 (should be > 0)");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
