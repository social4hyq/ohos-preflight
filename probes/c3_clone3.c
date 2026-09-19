// probes/c3_clone3.c
//
// 【测试目的】clone3() 系统调用。
//
// 【关联项目】通用
//
// 【上游调用链】
// C 运行时 / 工具链进程创建
//   → clone3(&args, sizeof(struct clone_args))             glibc 2.28+ / musl
//     → 内核 clone3(2)
//       → seccomp filter → SIGSYS（Signal 31）
//
// 【影响】clone3 是 glibc 2.28+ 及部分工具链创建线程/进程的首选接口，提供扩展的 clone_args 结构体。在 HarmonyOS 上调用导致 SIGSYS（core dump），影响依赖 clone3 的 C 运行时和工具链。
//
#define _GNU_SOURCE
#include <errno.h>
#include <linux/sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef SYS_clone3
#define SYS_clone3 435
#endif

struct probe_clone_args {
    uint64_t flags;
    uint64_t pidfd;
    uint64_t child_tid;
    uint64_t parent_tid;
    uint64_t exit_signal;
    uint64_t stack;
    uint64_t stack_size;
    uint64_t tls;
};

int main(void) {
    struct probe_clone_args args = {0};
    args.exit_signal = SIGCHLD;
    long pid = syscall(SYS_clone3, &args, sizeof(args));
    if (pid < 0) {
        if (errno == ENOSYS) {
            fprintf(stderr, "clone3 ENOSYS\n");
        } else {
            perror("clone3");
        }
        return 1;
    }
    if (pid == 0) {
        _exit(0);
    }
    int status = 0;
    if (waitpid((pid_t)pid, &status, 0) < 0) {
        perror("waitpid");
        return 2;
    }
    return 0;
}

