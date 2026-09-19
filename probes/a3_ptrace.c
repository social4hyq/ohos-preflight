// probes/a3_ptrace.c
//
// 【测试目的】ptrace(PTRACE_TRACEME) 进程跟踪。
//
// 【关联项目】通用
//
// 【上游调用链】
// 调试器 attach (gdb / lldb / JSC inspector)
//   → ptrace(PTRACE_TRACEME, 0, NULL, NULL)
//     → 返回 EPERM（沙箱禁止 ptrace）
//
// 【影响】ptrace 是调试器（gdb、lldb）和性能分析工具（perf）的基础。HarmonyOS 应用沙箱拒绝 PTRACE_TRACEME，开发者无法在真机上调试 Bun/Node.js 进程。
//
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 2;
    }
    if (pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("PTRACE_TRACEME");
            _exit(1);
        }
        raise(SIGSTOP);
        _exit(0);
    }
    int status = 0;
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
        return 2;
    }
    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "child did not stop (status=0x%x)\n", status);
        return 1;
    }
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) < 0) {
        perror("PTRACE_DETACH");
        return 1;
    }
    waitpid(pid, &status, 0);
    return 0;
}

