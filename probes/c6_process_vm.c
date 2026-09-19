// probes/c6_process_vm.c
//
// 【测试目的】process_vm_readv/writev (270/271) — JSC 调试器跨进程读写。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// JSC 调试器跨进程内存读写 (Bun inspector)
//   → libc::process_vm_readv(pid, ...) / process_vm_writev(pid, ...)
//     → 内核 process_vm_readv(2) / process_vm_writev(2)
// WebKit: JSC Inspector 跨进程读取被调试进程内存
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/uio.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    int ok = 0;

    // Test process_vm_readv (270 on aarch64, 310 on x86_64)
    struct iovec local = { .iov_base = &ok, .iov_len = sizeof(ok) };
    struct iovec remote = { .iov_base = (void*)(0), .iov_len = 0 };
    long ret = syscall(SYS_process_vm_readv, getpid(), &local, 1, &remote, 1, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked process_vm_readv (SIGSYS)");
        return 1;
    }
    ok = (ret != -1 || errno != ENOSYS);

    // Test process_vm_writev (271 on aarch64, 311 on x86_64)
    sigsys_caught = 0;
    ret = syscall(SYS_process_vm_writev, getpid(), &local, 1, &remote, 1, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked process_vm_writev (SIGSYS)");
        return 1;
    }
    if (!ok && ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "process_vm_readv/writev not implemented");
        return 1;
    }
    return 0;
}

