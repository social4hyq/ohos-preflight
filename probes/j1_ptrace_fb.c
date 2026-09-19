// probes/j1_ptrace_fb.c
//
// 【测试目的】a3_ptrace 替代：SIGSEGV 自捕获 handler。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void crash_handler(int sig, siginfo_t *si, void *ctx) {
    _exit(0);
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = crash_handler,
                            .sa_flags = SA_SIGINFO };
    sigaction(SIGSEGV, &sa, NULL);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork failed");
        return 1;
    }
    if (pid == 0) {
        raise(SIGSEGV);
        _exit(2);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    fprintf(stderr, "child status=0x%x (expected handler exit 0)", status);
    return 1;
}

