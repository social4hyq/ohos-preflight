// probes/c18_ioctl.c
//
// 【测试目的】验证 ioctl raw syscall (29) 可用性及 __NR_ioctl 宏定义。
// Bun 通过 libc::syscall(SYS_ioctl, fd, FICLONE, src_fd) 调用 FICLONE
// 实现 node:fs copyFile() 的写时复制快速路径。
//
// 【关联项目】Bun
//
// 【上游调用链】Bun copyFile FICLONE 快速路径
//   src/sys/lib.rs:5573 ioctl_ficlone(dest_fd, src_fd)
//     → libc::syscall(SYS_ioctl, dest_fd,                          libc crate
//         FICLONE=0x40049409, src_fd)
//       → seccomp filter → SIGSYS
//
//   调用方：
//     src/runtime/node/node_fs.rs:5295,5311,8729 — fs.copyFile()
//
//   注：Bun 也大量使用 libc ioctl() 包装器（TIOCGWINSZ/TIOCSCTTY/
//   TIOCGPTN/FIONREAD），但 raw syscall 路径仅 FICLONE 一处。
//   此探针覆盖 raw syscall 路径；libc 包装器路径见 i3_pty / e7_unix_socket。
//
// 【影响】FICLONE ioctl 被拦截时，Bun fs.copyFile() 回退到
// copy_file_range/sendfile/read-write 逐字节拷贝，大文件拷贝性能大幅下降。
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>

// FICLONE = _IOW(0x94, 9, int) — 与 bun src/sys/lib.rs:5575 完全一致
#define FICLONE 0x40049409ul

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    // 创建临时文件，打开两次获得 src_fd 和 dest_fd，
    // 与 bun node_fs.rs copyFile 的调用模式一致
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = ".";
    char tmpl[256];
    snprintf(tmpl, sizeof(tmpl), "%s/ohos-preflight-c18-XXXXXX", tmpdir);
    int src_fd = mkstemp(tmpl);
    if (src_fd < 0) {
        fprintf(stderr, "mkstemp failed: %s", strerror(errno));
        return 2;
    }
    // 写入一些内容使文件非空（FICLONE 要求源文件有数据）
    if (write(src_fd, "ohos-preflight c18", 18) != 18) {
        fprintf(stderr, "write failed: %s", strerror(errno));
        close(src_fd);
        unlink(tmpl);
        return 2;
    }

    int dest_fd = open(tmpl, O_RDWR);
    if (dest_fd < 0) {
        fprintf(stderr, "open dest failed: %s", strerror(errno));
        close(src_fd);
        unlink(tmpl);
        return 2;
    }

    // 与 bun src/sys/lib.rs:5577-5583 完全一致的 raw syscall 模式:
    //   libc::syscall(SYS_ioctl, dest_fd, FICLONE, src_fd)
    long ret = syscall(SYS_ioctl, dest_fd, FICLONE, src_fd);

    close(src_fd);
    close(dest_fd);
    unlink(tmpl);

    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked ioctl (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "ioctl not implemented");
        return 1;
    }
    // 其他错误 (EOPNOTSUPP/EINVAL) 说明 syscall 正常执行，
    // 只是文件系统不支持 reflink — 这是预期的，syscall 本身可用
    return 0;
}
