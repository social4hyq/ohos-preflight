// probes/j8_execveat_fb.c
//
// 【测试目的】c1_execveat_sym 替代：syscall(SYS_execveat) 直接发起调用。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();
    if (pid < 0) { fprintf(stderr, "fork: %m"); return 1; }
    if (pid == 0) {
        char *argv[] = { (char *)"sh", (char *)"-c", (char *)"exit 42", NULL };
        char *envp[] = { NULL };
        syscall(SYS_execveat, AT_FDCWD, "/bin/sh", argv, envp, 0);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %m");
        return 1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 42) return 0;
    fprintf(stderr, "child status=0x%x (expected exit 42)", status);
    return 1;
}

