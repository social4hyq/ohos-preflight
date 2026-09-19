// probes/c7_statx.c
//
// 【测试目的】statx 扩展 stat — Bun sys/lib.rs:2265。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun fs.stat() / Bun.file().size 文件元数据查询:
//   → workaround_symbols.statx(dir, path, flags, mask, buf)    bun/src/workaround_missing_symbols.zig
//     → std.os.linux.statx(dirfd, path, flags, mask, buf)
//       → 内核 statx(2)
// 
// WebKit 文件创建时间查询:
//   → statx(-1, path, 0, STATX_BTIME, &fileInfo)               FileSystemPOSIX.cpp:103-105
//   → HAVE_STATX 编译时检测                                     OptionsCommon.cmake:321
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

// Minimal statx struct matching kernel layout (enough for status check)
struct statx_timestamp { long tv_sec; unsigned int tv_nsec; int __reserved; };
struct statx_buf {
    unsigned int stx_mask, stx_blksize, stx_attributes;
    unsigned int stx_nlink, stx_uid, stx_gid;
    unsigned short stx_mode; unsigned short __spare0[1];
    unsigned int stx_ino, stx_size, stx_blocks, stx_attributes_mask;
    struct statx_timestamp stx_atime, stx_btime, stx_ctime, stx_mtime;
    unsigned int stx_rdev_major, stx_rdev_minor, stx_dev_major, stx_dev_minor;
    unsigned long long __spare2[14];
};

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    // AT_EMPTY_PATH=0x1000 statx via /proc/self to verify kernel supports it
    struct statx_buf buf;
    long ret = syscall(SYS_statx, AT_FDCWD, "/dev/null", 0, 0x7ffU, &buf);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked statx (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "statx not implemented");
        return 1;
    }
    return 0;
}

