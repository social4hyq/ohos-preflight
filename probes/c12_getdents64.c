// probes/c12_getdents64.c
//
// 【测试目的】getdents64 目录项读取 — Bun linux_syscall.rs:509。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun fs.readdir() / glob / import 模块解析
//   → linux_syscall::getdents64(fd, buf, len)              bun/src/sys/linux_syscall.rs:500
//     → libc::syscall(SYS_getdents64, fd, buf, len)        libc crate
//       → 内核 getdents64(2)
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    int fd = open(".", O_RDONLY | O_DIRECTORY);
    if (fd < 0) { fprintf(stderr, "cannot open ."); return 2; }

    unsigned char buf[1024];
    long ret = syscall(SYS_getdents64, fd, buf, sizeof(buf));
    close(fd);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked getdents64 (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "getdents64 not implemented");
        return 1;
    }
    return 0;
}

