// probes/a1_seccomp_unotify.c
//
// 【测试目的】验证 seccomp 用户态通知机制可用性。
// seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_NEW_LISTENER) 创建一个
// 监听 fd，当被拦截的 syscall 触发时，内核通知用户态处理程序决定是否放行。
//
// 【上游调用链】vite-plus
//   vite-task/crates/fspy_seccomp_unotify
//     → seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_NEW_LISTENER)
//       → seccompiler (rust-vmm/seccompiler) — 将 seccomp rule 编译为 BPF filter
//         → ioctl(listener_fd, SECCOMP_IOCTL_NOTIF_RECV) — 接收内核通知
//           → ioctl(listener_fd, SECCOMP_IOCTL_NOTIF_SEND) — 返回裁决结果
//
// 【影响】vite-plus fspy 模块使用 seccomp 用户态通知实现文件系统操作拦截
// （拦截 open/write/unlink 等调用，重定向到虚拟文件系统）。HarmonyOS 上
// SECCOMP_SET_MODE_FILTER 返回 EINVAL，fspy 完全无法工作。
//
// Exit codes:
//   0 = listener fd obtained
//   1 = seccomp syscall rejected the filter
//   2 = PR_SET_NO_NEW_PRIVS unavailable (precondition)

#define _GNU_SOURCE
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void) {
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        fprintf(stderr, "PR_SET_NO_NEW_PRIVS not supported\n");
        return 2;
    }
    // Filter returns ALLOW so the listener fd is created but no syscall
    // actually triggers user-notif (otherwise close(fd)/exit would deadlock
    // waiting on a consumer). We only verify the kernel accepts
    // SECCOMP_FILTER_FLAG_NEW_LISTENER + a valid filter program.
    struct sock_filter filter[] = {
        { .code = 0x20, .k = 0 },            // BPF_LD|BPF_W|BPF_ABS, arch
        { .code = 0x06, .k = 0x7fff0000 },   // BPF_RET, SECCOMP_RET_ALLOW
    };
    struct sock_fprog prog = { .len = 2, .filter = filter };
    int fd = syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER,
                     SECCOMP_FILTER_FLAG_NEW_LISTENER, &prog);
    if (fd < 0) {
        perror("seccomp SET_MODE_FILTER");
        return 1;
    }
    close(fd);
    return 0;
}
