// probes/j3_clone3_fb.c
//
// 【测试目的】c3_clone3 替代：clone(2) + CLONE_PIDFD 等价路径。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int child_fn(void *arg) { _exit(42); }

int main(void) {
    char *stack = malloc(65536);
    if (!stack) { fprintf(stderr, "malloc failed"); return 1; }

    pid_t pid = clone(child_fn, stack + 65536, SIGCHLD, NULL);
    if (pid < 0) {
        fprintf(stderr, "clone failed: %m");
        free(stack);
        return 1;
    }
    int status;
    waitpid(pid, &status, 0);
    free(stack);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 42) return 0;
    fprintf(stderr, "child status 0x%x (expected exit 42)", status);
    return 1;
}

