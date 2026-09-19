// probes/k4_vfork_child_fcntl_cloexec.c
//
// 【测试目的】验证 vfork() 子进程里 fcntl(fd, F_SETFD, FD_CLOEXEC) 是否
// 真的在随后的 execve() 里生效。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// posix_spawn_bun() 的 Linux-非-OHOS 分支 vfork 后设置子进程 fd 属性:
//   → vfork()
//     → fcntl(fd, F_SETFD, FD_CLOEXEC)     — bun-spawn.cpp:415 附近注释所指
//       → execve(...)
//         → 若 CLOEXEC 被忽略，该 fd 泄漏进新程序镜像
//
// 【为何需要】bun-spawn.cpp:415 的注释断言"OHOS 上 vfork 子进程里
// fcntl(F_SETFD) 会被忽略"，但该结论从未有独立探针验证过 —— 本探针直接
// 复现这一具体场景（vfork 子进程内 fcntl 后立即 execve 自身的一份拷贝，
// 检查目标 fd 在新镜像里是否真的关闭了）。
//
// 复用 k1 的"以特殊 argv 重新进入自身、扮演子进程角色"手法，但这里不需要
// 先拷贝文件（不测签名，只测 fd 属性），直接 execve(/proc/self/exe) 即可。
//
// Exit: 0=pass (CLOEXEC 生效，fd 在新镜像里已关闭 — bun-spawn.cpp:415 的
//              担忧不成立，vfork 分支的额外处理可以简化)
//       1=fail (fd 在新镜像里仍然打开 — fcntl 确实被忽略，需要继续手工处理)
//       2=unsupported (vfork/execve 本身失败，测不出这个更具体的问题)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD_FLAG "--k4-child"

extern char **environ;

int main(int argc, char **argv) {
    if (argc > 2 && strcmp(argv[1], CHILD_FLAG) == 0) {
        int fd = atoi(argv[2]);
        // If FD_CLOEXEC was honored across the vfork child's execve, this
        // fd number must now be closed in this fresh image.
        int flags = fcntl(fd, F_GETFD);
        return (flags != -1) ? 1 : 0;
    }

    char self_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len < 0) {
        fprintf(stderr, "readlink(/proc/self/exe): %s", strerror(errno));
        return 2;
    }
    self_path[len] = '\0';

    int probe_pipe[2];
    if (pipe(probe_pipe) != 0) {
        fprintf(stderr, "pipe: %s", strerror(errno));
        return 2;
    }
    close(probe_pipe[0]); // only the write end's fd number matters here

    char fd_str[16];
    snprintf(fd_str, sizeof(fd_str), "%d", probe_pipe[1]);
    // All argv strings must be fully prepared before vfork() -- the child
    // shares the parent's memory until it execve()s, so no allocations or
    // libc calls with side effects are safe in between.
    char *child_argv[] = { self_path, (char *)CHILD_FLAG, fd_str, NULL };

    pid_t pid = vfork();
    if (pid == 0) {
        fcntl(probe_pipe[1], F_SETFD, FD_CLOEXEC);
        execve(self_path, child_argv, environ);
        _exit(125);
    }
    if (pid < 0) {
        fprintf(stderr, "vfork: %s", strerror(errno));
        close(probe_pipe[1]);
        return 2;
    }
    close(probe_pipe[1]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s", strerror(errno));
        return 2;
    }
    if (!WIFEXITED(status)) {
        fprintf(stderr, "vfork+execve child ended abnormally (raw status %d)", status);
        return 2;
    }
    int code = WEXITSTATUS(status);
    if (code == 125) {
        fprintf(stderr, "execve inside vfork child failed -- can't test this scenario");
        return 2;
    }
    if (code == 1) {
        fprintf(stderr, "fd survived execve despite FD_CLOEXEC set in vfork child -- fcntl(F_SETFD) is ignored there");
        return 1;
    }
    if (code != 0) {
        fprintf(stderr, "child-mode exited with unexpected status %d", code);
        return 2;
    }
    return 0;
}
