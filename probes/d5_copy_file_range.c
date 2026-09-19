// probes/d5_copy_file_range.c
//
// 【测试目的】copy_file_range (285) 内核态拷贝 — Bun 大文件。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.write() / Bun cp 命令内核态零拷贝
//   → copy_file_range(in_fd, off, out_fd, off2,            bun/src/sys/copy_file.rs:329
//       len, flags)
//     → libc::syscall(SYS_copy_file_range, in_fd,          libc crate
//         off, out_fd, off2, len, flags)
//       → 内核 copy_file_range(2)
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

    // Test with invalid fd to check syscall existence (expect EBADF)
    long ret = syscall(SYS_copy_file_range, -1, 0, -1, 0, 0, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked copy_file_range (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "copy_file_range not implemented");
        return 1;
    }
    return 0;
}

