// 替代：clone() 覆盖 clone3 的常见用法（clone_args 扩展功能丢失）
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int child_fn(void *arg) { _exit(42); }

int spawn_via_clone(void) {
    char *stack = malloc(65536);
    pid_t pid = clone(child_fn, stack + 65536,
                      SIGCHLD | CLONE_VM | CLONE_VFORK, NULL);
    int status;
    waitpid(pid, &status, 0);
    free(stack);
    return WIFEXITED(status) && WEXITSTATUS(status) == 42 ? 0 : 1;
}
// 局限：clone3 独有的 set_tid / cgroup / pidfd 字段无法在 clone() 中获得
// TODO: 待 HarmonyOS 放行 clone3 后切回 syscall(SYS_clone3, &args, sizeof args)
