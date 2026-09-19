// probes/c8_renameat2.c
//
// 【测试目的】renameat2 原子 rename — Bun sys/lib.rs:2519。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun fs.rename() 原子重命名
//   → libc::syscall(SYS_renameat2, olddir, oldpath, newdir, newpath, flags)
//     → 内核 renameat2(2)
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

    // Test with nonexistent paths to verify syscall exists (expect ENOENT)
    long ret = syscall(SYS_renameat2, AT_FDCWD, "/nonexistent_src",
                       AT_FDCWD, "/nonexistent_dst", 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked renameat2 (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "renameat2 not implemented");
        return 1;
    }
    return 0;
}

