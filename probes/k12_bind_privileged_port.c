// probes/k12_bind_privileged_port.c
//
// 【测试目的】验证不带 CAP_NET_BIND_SERVICE 时能否 bind() 到特权端口
// （<1024）。c17_bind 只测端口 0（内核随机分配的高位端口），覆盖不到
// 这个更具体的限制。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// Bun cluster 模块特权端口监听:
//   → net.Server.listen(80) / cluster 主进程转发 fd
//     → bind(fd, {port: 80, ...})
//       → 缺 CAP_NET_BIND_SERVICE → EACCES        OHOS_TEST_STATUS.md 硬限制 #4
//
// 【为何需要】目前记录在案是"缺 CAP_NET_BIND_SERVICE"这个平台限制，
// 但从未有独立探针复现过具体的 errno 和端口号，本探针把它固化成可重跑
// 的回归检查。
//
// Exit: 0=pass (特权端口 bind 成功 -- cluster 特权端口限制已解除)
//       1=fail (EACCES -- 限制仍然存在)
//       2=unsupported (socket() 本身失败，或该端口已被占用导致误判)

#define _GNU_SOURCE
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

#define PRIVILEGED_PORT 7 /* echo -- long-reserved, unlikely to collide with a running service */

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket(): %s", strerror(errno));
        return 2;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PRIVILEGED_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };
    int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    int saved_errno = errno;
    close(fd);

    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked bind (SIGSYS)");
        return 2;
    }
    if (ret == 0) {
        return 0;
    }
    if (saved_errno == EADDRINUSE) {
        fprintf(stderr, "port %d already in use -- inconclusive, pick a different port", PRIVILEGED_PORT);
        return 2;
    }
    if (saved_errno == EACCES) {
        fprintf(stderr, "bind() to privileged port %d refused (EACCES, no CAP_NET_BIND_SERVICE)", PRIVILEGED_PORT);
        return 1;
    }
    fprintf(stderr, "bind() to privileged port failed with unexpected errno: %s", strerror(saved_errno));
    return 2;
}
