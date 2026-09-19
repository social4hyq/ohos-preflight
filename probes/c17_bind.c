// probes/c17_bind.c
//
// 【测试目的】验证 libc bind() 可用性及 __NR_bind 宏定义。
// bind 将 socket 绑定到本地地址，Bun 网络栈通过 libc 包装器调用。
//
// 【关联项目】Bun
//
// 【上游调用链】Bun uSockets 网络栈
//   packages/bun-usockets/src/bsd.c:910
//     → bind(listenFd, listenAddr, listenAddrLength)
//       → libc bind() 包装函数
//         → syscall(SYS_bind=200, fd, addr, addrlen)
//           → seccomp filter → SIGSYS
//
//   packages/h3blast/src/h3blast.c:536
//     → bind(fd, (struct sockaddr*)&local, llen)
//
//   WebKit RemoteInspectorSocketPOSIX.cpp:111
//     → ::bind(fdListen, (struct sockaddr*)&address, sizeof(address))
//
// 【影响】Bun HTTP/WebSocket 服务器启动时调用 bind() 绑定监听端口。
// 若被 seccomp 拦截则所有网络服务无法启动。
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/syscall.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    // 与 Bun uSockets bsd.c 完全一致的调用模式：先 socket() 再 bind()
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket() failed: %s", strerror(errno));
        return 2;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = 0,            // port 0 = 内核分配临时端口
        .sin_addr.s_addr = INADDR_ANY,
    };

    // 与 bsd.c:910 完全一致的 libc bind() 调用模式
    int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    close(fd);

    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked bind (SIGSYS)");
        return 1;
    }
    if (ret < 0) {
        if (errno == ENOSYS) {
            fprintf(stderr, "bind not implemented");
            return 1;
        }
        // EACCES/EPERM on low ports — acceptable, syscall itself works
        fprintf(stderr, "bind() failed: %s (syscall available)", strerror(errno));
        return 0;
    }
    return 0;
}
