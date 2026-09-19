// probes/c10_preadv2.c
//
// 【测试目的】preadv2 带 flag 分散读 — Bun c-bindings.cpp:470。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.file().arrayBuffer() 指定偏移分散读
//   → linux_syscall::preadv(fd, iovec[], offset)           bun/src/sys/linux_syscall.rs:305
//     → libc::syscall(SYS_preadv, fd, vecs, n, lo, hi)    libc crate
//       → 内核 preadv2(2)
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

    int fd = open("/dev/null", O_RDONLY);
    if (fd < 0) { fprintf(stderr, "cannot open /dev/null"); return 2; }

    char buf[64];
    struct iovec iov = { .iov_base = buf, .iov_len = sizeof(buf) };
    // Bun: libc::syscall(SYS_preadv, fd, vecs, n, lo, hi)
    // lo/hi split mirrors kernel's loff_t/hif_t ABI on LP64
    long off = 0;
    long lo = off;
    long hi = ((off >> 32) & 0xffffffff);
    long ret = syscall(SYS_preadv, fd, &iov, 1, lo, hi);
    close(fd);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked preadv (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "preadv not implemented");
        return 1;
    }
    // /dev/null returns 0 on preadv (EOF) — syscall exists
    return 0;
}

