// probes/c4_openat2.c
//
// 【测试目的】验证 openat2 (syscall 437) 可用性。
// openat2 在打开文件时指定 RESOLVE_* 标志，原子性地防止符号链接路径逃逸。
//
// 【上游调用链】Bun
//   linux_syscall::openat2_beneath(dir, path, flags, mode)   linux_syscall.rs:93
//     → rustix::fs::openat2(dir, path, oflags, mode,
//         ResolveFlags::BENEATH)
//       → libc::syscall(SYS_openat2=437, ...)
//         → seccomp filter → SIGSYS
//
// 【影响】Bun 使用 openat2 + RESOLVE_BENEATH 打开文件以防止路径逃逸攻击。
// HarmonyOS seccomp 拦截该调用（SIGSYS），Bun 回退到 openat() 但丢失安全约束。
// 降级方案：openat() + O_NOFOLLOW + fstatat 路径段逐级扫描（j4_openat2_fb），
// 存在 TOCTOU 窗口。
//
// Exit codes:
//   0 = syscall accepted (returns ENOENT with /nonexistent → exists)
//   1 = seccomp SIGSYS or ENOSYS

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <linux/openat2.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    struct open_how how = {
        .flags = O_RDONLY | O_CLOEXEC,
        .mode = 0,
        .resolve = 0x40 /* RESOLVE_BENEATH */,
    };
    long ret = syscall(SYS_openat2, AT_FDCWD, ".", &how, sizeof(how));
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked openat2 (SIGSYS)");
        return 1;
    }
    if (ret >= 0) close((int)ret);
    return 0;
}
