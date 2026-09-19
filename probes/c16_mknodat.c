// probes/c16_mknodat.c
//
// 【测试目的】验证 mknodat (syscall 33) 可用性。
// mknodat 在指定目录下创建文件系统节点（普通文件、设备文件、FIFO 等）。
//
// 【关联项目】通用系统调用（注：Bun / WebKit 均未直接调用此 syscall，
// 仅 libarchive 内有 mknod() 使用；此探针为通用内核能力覆盖）
//
// 【上游调用链】（POSIX 标准路径，非 Bun 特有）
//   mknodat(AT_FDCWD, path, mode, dev)
//     → libc mknodat() 包装函数
//       → syscall(SYS_mknodat=33, dirfd, path, mode, dev)
//         → seccomp filter → SIGSYS
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    // mknodat on /dev/null (already exists as char device) — fails EEXIST, no side effect
    long ret = syscall(SYS_mknodat, AT_FDCWD, "/dev/null", S_IFREG, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked mknodat (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "mknodat not implemented");
        return 1;
    }
    return 0;
}
