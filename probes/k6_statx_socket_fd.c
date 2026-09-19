// probes/k6_statx_socket_fd.c
//
// 【测试目的】验证 statx(2) 对 socket-backed fd 是否仍然返回 EBADF
// （c7_statx 只测通用文件路径，覆盖不到这个更具体的 fd 类型问题）。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// sys/lib.rs 的 statx 包装:
//   → lx::statx(fd, "", AT_EMPTY_PATH, mask, buf)             sys/lib.rs:2322
//     → 若 fd 是 socket: OHOS 返回 EBADF（其它平台/内核在这种 fd 类型上
//       通常返回 ENOSYS/EOPNOTSUPP，OHOS 单独多出这一种失败形状）
//       → sys/lib.rs:2358 把 EBADF 和 ENOSYS/EOPNOTSUPP/EPERM/EINVAL
//         一起折进 statx_fallback()（改用 fstat）
//
// 【为何需要】这条 fallback 分支判据依赖真机验证过的一次性观察
//（"verified on-device" 注释），本探针把它变成可重复运行的回归检查。
//
// Exit: 0=pass (对 socket fd 调用 statx 不再返 EBADF -- sys/lib.rs:2358
//              里 OHOS 专属的 EBADF 特判可以删除)
//       1=fail (仍然返回 EBADF -- 特判仍然必需)
//       2=unsupported (socketpair 本身失败，或 statx 系统调用整体不可用)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

// Minimal statx buffer, layout-compatible with the kernel struct (same
// approach as probes/c7_statx.c -- only enough fields to get a status).
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

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        fprintf(stderr, "socketpair: %s", strerror(errno));
        return 2;
    }

    // Sanity: plain fstat must succeed on a socket fd (this is the
    // fallback path's whole premise -- if fstat itself fails here, the
    // comparison this probe makes is meaningless).
    struct stat st;
    if (fstat(sv[0], &st) != 0) {
        fprintf(stderr, "fstat(socket fd) unexpectedly failed: %s -- can't evaluate statx fallback premise", strerror(errno));
        close(sv[0]); close(sv[1]);
        return 2;
    }

    struct statx_buf buf;
    long ret = syscall(SYS_statx, sv[0], "", 0x1000 /* AT_EMPTY_PATH */, 0x7ffU, &buf);
    int saved_errno = errno;
    close(sv[0]);
    close(sv[1]);

    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked statx (SIGSYS)");
        return 2;
    }
    if (ret == 0) {
        return 0; // statx now works on socket fds -- bug fixed
    }
    if (saved_errno == EBADF) {
        fprintf(stderr, "statx(socket fd) still returns EBADF while fstat succeeds on the same fd");
        return 1;
    }
    fprintf(stderr, "statx(socket fd) failed with unexpected errno: %s", strerror(saved_errno));
    return 2;
}
