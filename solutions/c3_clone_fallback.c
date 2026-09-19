// solutions/c3_clone_fallback.c
//
// 替代 clone3()：使用传统的 clone() 系统调用。
// HarmonyOS 沙箱限制 clone3，但 clone() 可用。
//
// 验证：用 clone() 创建子进程（共享信号处理），验证返回。

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int child_fn(void *arg) {
    _exit(42);
}

int main(void) {
    // 64KB 栈空间
    char *stack = malloc(65536);
    if (!stack) return 1;

    pid_t pid = clone(child_fn, stack + 65536,
                      SIGCHLD | CLONE_VM | CLONE_VFORK, NULL);
    if (pid < 0) {
        fprintf(stderr, "clone failed: %s", strerror(errno));
        free(stack);
        return 1;
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid failed: %s", strerror(errno));
        free(stack);
        return 1;
    }

    free(stack);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 42) {
        fprintf(stderr, "unexpected child exit: %d", status);
        return 1;
    }
    return 0;
}
