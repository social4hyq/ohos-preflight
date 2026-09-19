// probes/c11_pwritev2.c
//
// 【测试目的】pwritev2 带 flag 聚集写 — Bun c-bindings.cpp:478。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.write() 指定偏移聚集写
//   → linux_syscall::pwritev(fd, iovec[], offset)          bun/src/sys/linux_syscall.rs:335
//     → libc::syscall(SYS_pwritev, fd, vecs, n, lo, hi)   libc crate
//       → 内核 pwritev2(2)
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <fcntl.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    int fd = open("/dev/null", O_WRONLY);
    if (fd < 0) { fprintf(stderr, "cannot open /dev/null"); return 2; }

    struct iovec iov = { .iov_base = "test", .iov_len = 4 };
    // Bun: libc::syscall(SYS_pwritev, fd, vecs, n, lo, hi)
    long lo = 0;
    long hi = ((0 >> 32) & 0xffffffff);
    long ret = syscall(SYS_pwritev, fd, &iov, 1, lo, hi);
    close(fd);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked pwritev (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "pwritev not implemented");
        return 1;
    }
    return 0;
}

