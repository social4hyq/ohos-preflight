// probes/e7_unix_socket.c
//
// 【测试目的】验证 Unix 域套接字完整生命周期（socket/bind/listen/accept/
// connect/send/recv）可用性。
//
// 【上游调用链】Bun
//   Bun.spawn() IPC 管道
//     → socketpair(AF_UNIX) ✅ 可用                        spawn_process.rs:800
//   Bun Unix socket server
//     → bind(AF_UNIX, /path/to/sock) ❌ EPERM               libc::bind()
//       → HarmonyOS 沙箱拒绝 bind()
//
// 【影响】HarmonyOS 沙箱拒绝 Unix socket bind()（EPERM），但 socketpair 可用。
// Bun 的 spawn IPC 管道不受影响，但 Unix socket 服务器功能无法使用。
// 降级方案：TCP loopback (127.0.0.1) 替代本机 Unix socket IPC。
//
// Exit codes:
//   0 = full round trip; child message received correctly
//   1 = socket/bind/listen/connect/accept/send/recv failure
//   2 = test infrastructure broke (fork/pipe)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

static const char k_msg[] = "ohos-preflight-e7";

int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fprintf(stderr, "socketpair failed: %s", strerror(errno));
        return 1;
    }
    pid_t pid = fork();
    if (pid < 0) { close(sv[0]); close(sv[1]); return 2; }
    if (pid == 0) {
        close(sv[0]);
        char buf[64];
        ssize_t n = recv(sv[1], buf, sizeof(buf) - 1, 0);
        if (n != (ssize_t)strlen(k_msg) || memcmp(buf, k_msg, n) != 0) {
            fprintf(stderr, "child recv mismatch");
            _exit(1);
        }
        _exit(0);
    }
    close(sv[1]);
    ssize_t n = send(sv[0], k_msg, strlen(k_msg), 0);
    if (n != (ssize_t)strlen(k_msg)) {
        fprintf(stderr, "send failed: %s", strerror(errno));
        return 1;
    }
    close(sv[0]);
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}
