// probes/i6_sendfile.c
//
// 【测试目的】sendfile() 零拷贝文件传输。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.serve() 静态文件零拷贝发送
//   → linux_syscall::sendfile(out_fd, in_fd,               bun/src/sys/linux_syscall.rs:421
//       offset, count)
//     → libc::syscall(71, out_fd, in_fd, offset, count)    aarch64 硬编码 SYS_sendfile = 71
//       → 内核 sendfile(2)
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    // Bun: libc::syscall(71, out_fd, in_fd, offset, count)
    // on aarch64, SYS_sendfile == 71
    long ret = syscall(71, -1, -1, 0, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked sendfile (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "sendfile not implemented");
        return 1;
    }
    // EBADF on invalid fds → syscall exists
    return 0;
}

