// probes/i7_posix_spawn.c
//
// 【测试目的】posix_spawn() 创建子进程。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.spawn() / Bun Shell / bun run 子进程创建
//   → posix_spawn::spawn_z(opts, actions, attr,            bun/src/spawn_sys/posix_spawn.rs
//       path, argv, envp)
//     → posix_spawn_bun(request)                            C++ bun-spawn.cpp
//       → vfork() + execve()
//         → 内核 clone(CLONE_VFORK) + execve(2)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef POSIX_SPAWN_CLOEXEC_DEFAULT
#define POSIX_SPAWN_CLOEXEC_DEFAULT 0x4000
#endif

extern char **environ;

int main(void) {
    pid_t pid;
    char *argv[] = {"/bin/true", NULL};
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;

    posix_spawn_file_actions_init(&actions);
    posix_spawnattr_init(&attr);

    // Bun: POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSID
    int flags = POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGDEF;
    posix_spawnattr_setflags(&attr, flags);

    int rc = posix_spawn(&pid, "/bin/true", &actions, &attr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);

    if (rc != 0) {
        fprintf(stderr, "posix_spawn failed: %s\n", strerror(rc));
        return 1;
    }

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "/bin/true exited with %d\n", WEXITSTATUS(status));
        return 1;
    }
    return 0;
}

